// src/platform/signalDaemon.cpp
#include "signalDaemon.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

namespace Texxy {

namespace {
constexpr char kTick = 1;  // single byte used to tickle the notifier
}

std::array<signalDaemon::Pipe, signalDaemon::kSignalCount> signalDaemon::pipes_{{
    {SIGHUP},
    {SIGTERM},
    {SIGINT},
    {SIGQUIT},
}};

// prefer CLOEXEC and NONBLOCK for safety and responsiveness
bool signalDaemon::makeSocketPair(std::array<int, 2>& fds) {
#ifdef SOCK_CLOEXEC
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, fds.data()) == 0)
        return true;
#endif
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds.data()) != 0)
        return false;

    for (int& fd : fds) {
        int flags = ::fcntl(fd, F_GETFD, 0);
        if (flags != -1)
            ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
        int status = ::fcntl(fd, F_GETFL, 0);
        if (status != -1)
            ::fcntl(fd, F_SETFL, status | O_NONBLOCK);
    }
    return true;
}

void signalDaemon::closePair(std::array<int, 2>& fds) noexcept {
    for (int& fd : fds) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
}

// drain everything available to avoid repeated wakeups
void signalDaemon::drainFd(int fd) noexcept {
    char buf[256];
    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0)
            continue;
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        break;
    }
}

signalDaemon::signalDaemon(QObject* parent) : QObject(parent) {
    for (int i = 0; i < kSignalCount; ++i) {
        Pipe& pipe = pipes_.at(i);
        if (!makeSocketPair(pipe.fds))
            continue;

        auto* notifier = new QSocketNotifier(pipe.fds[1], QSocketNotifier::Read, this);
        const int idx = i;
        connect(notifier, &QSocketNotifier::activated, this, [this, idx]() { handleSignal(idx); });
        notifiers_.at(i) = notifier;
    }
}

signalDaemon::~signalDaemon() {
    // notifiers are parented to this and auto-deleted by QObject dtor
    for (Pipe& pipe : pipes_)
        closePair(pipe.fds);
}

// async-signal-safe handlers write a single byte and return
void signalDaemon::signalHandler(int sig) {
    if (Pipe* pipe = pipeForSignal(sig)) {
        const char b = kTick;
        (void)::write(pipe->fds[0], &b, sizeof(b));
    }
}

// each handler disables the notifier, drains all pending bytes, emits merged signal, then reenables
void signalDaemon::handleSignal(int index) {
    if (index < 0 || index >= kSignalCount)
        return;

    QSocketNotifier* notifier = notifiers_.at(index);
    if (!notifier)
        return;

    notifier->setEnabled(false);
    drainFd(pipes_.at(index).fds[1]);
    emit sigQUIT();
    notifier->setEnabled(true);
}

// install handlers with SA_RESTART to reduce EINTR and use an empty mask for minimal latency
bool signalDaemon::watchSignal(Pipe& pipe) {
    if (pipe.fds[0] < 0 || pipe.fds[1] < 0)
        return false;

    struct sigaction sa{};
    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = &signalDaemon::signalHandler;
    return (::sigaction(pipe.sig, &sa, nullptr) == 0);
}

void signalDaemon::watchUnixSignals() {
    for (Pipe& pipe : pipes_) {
        [[maybe_unused]] const bool ok = watchSignal(pipe);
    }
}

signalDaemon::Pipe* signalDaemon::pipeForSignal(int sig) {
    for (Pipe& pipe : pipes_) {
        if (pipe.sig == sig)
            return &pipe;
    }
    return nullptr;
}

}  // namespace Texxy
