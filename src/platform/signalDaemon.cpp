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
    if (makeSocketPair(sighupFd)) {
        snHup_ = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
        connect(snHup_, &QSocketNotifier::activated, this, &signalDaemon::handleSigHup);
    }

    if (makeSocketPair(sigtermFd)) {
        snTerm_ = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
        connect(snTerm_, &QSocketNotifier::activated, this, &signalDaemon::handleSigTerm);
    }

    if (makeSocketPair(sigintFd)) {
        snInt_ = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
        connect(snInt_, &QSocketNotifier::activated, this, &signalDaemon::handleSigINT);
    }

    if (makeSocketPair(sigquitFd)) {
        snQuit_ = new QSocketNotifier(sigquitFd[1], QSocketNotifier::Read, this);
        connect(snQuit_, &QSocketNotifier::activated, this, &signalDaemon::handleSigQUIT);
    }
}

signalDaemon::~signalDaemon() {
    // notifiers are parented to this and auto-deleted by QObject dtor
    closePair(sighupFd);
    closePair(sigtermFd);
    closePair(sigintFd);
    closePair(sigquitFd);
}

// async-signal-safe handlers write a single byte and return
void signalDaemon::hupSignalHandler(int) {
    const char b = kTick;
    (void)::write(sighupFd[0], &b, sizeof(b));
}

void signalDaemon::termSignalHandler(int) {
    const char b = kTick;
    (void)::write(sigtermFd[0], &b, sizeof(b));
}

void signalDaemon::intSignalHandler(int) {
    const char b = kTick;
    (void)::write(sigintFd[0], &b, sizeof(b));
}

void signalDaemon::quitSignalHandler(int) {
    const char b = kTick;
    (void)::write(sigquitFd[0], &b, sizeof(b));
}

// each handler disables the notifier, drains all pending bytes, emits merged signal, then reenables
void signalDaemon::handleSigHup() {
    if (!snHup_)
        return;
    snHup_->setEnabled(false);
    drainFd(sighupFd[1]);
    emit sigQUIT();
    snHup_->setEnabled(true);
}

void signalDaemon::handleSigTerm() {
    if (!snTerm_)
        return;
    snTerm_->setEnabled(false);
    drainFd(sigtermFd[1]);
    emit sigQUIT();
    snTerm_->setEnabled(true);
}

void signalDaemon::handleSigINT() {
    if (!snInt_)
        return;
    snInt_->setEnabled(false);
    drainFd(sigintFd[1]);
    emit sigQUIT();
    snInt_->setEnabled(true);
}

void signalDaemon::handleSigQUIT() {
    if (!snQuit_)
        return;
    snQuit_->setEnabled(false);
    drainFd(sigquitFd[1]);
    emit sigQUIT();
    snQuit_->setEnabled(true);
}

// install handlers with SA_RESTART to reduce EINTR and use an empty mask for minimal latency
bool signalDaemon::watchSignal(int sig) {
    struct sigaction sa{};
    switch (sig) {
        case SIGHUP:
            if (!snHup_)
                return false;
            sa.sa_handler = &signalDaemon::hupSignalHandler;
            break;
        case SIGTERM:
            if (!snTerm_)
                return false;
            sa.sa_handler = &signalDaemon::termSignalHandler;
            break;
        case SIGINT:
            if (!snInt_)
                return false;
            sa.sa_handler = &signalDaemon::intSignalHandler;
            break;
        case SIGQUIT:
            if (!snQuit_)
                return false;
            sa.sa_handler = &signalDaemon::quitSignalHandler;
            break;
        default:
            return false;
    }

    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return (::sigaction(sig, &sa, nullptr) == 0);
}

void signalDaemon::watchUnixSignals() {
    [[maybe_unused]] const bool okH = watchSignal(SIGHUP);
    [[maybe_unused]] const bool okT = watchSignal(SIGTERM);
    [[maybe_unused]] const bool okI = watchSignal(SIGINT);
    [[maybe_unused]] const bool okQ = watchSignal(SIGQUIT);
}

}  // namespace Texxy
