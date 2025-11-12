// src/platform/signalDaemon.h
#pragma once

#include <QObject>
#include <QSocketNotifier>

#include <array>

namespace Texxy {

// single tiny byte gets pushed from async-signal-safe handlers into a socketpair
// we intentionally emit a single merged sigQUIT Qt signal for all watched Unix signals
class signalDaemon : public QObject {
    Q_OBJECT
   public:
    explicit signalDaemon(QObject* parent = nullptr);
    ~signalDaemon() override;

    void watchUnixSignals();

   signals:
    void sigQUIT();

   private:
    struct Pipe {
        int sig;
        std::array<int, 2> fds{-1, -1};
    };

    static constexpr int kSignalCount = 4;

    [[nodiscard]] bool watchSignal(Pipe& pipe);
    void handleSignal(int index);

    [[nodiscard]] static Pipe* pipeForSignal(int sig);
    static void signalHandler(int sig);
    static bool makeSocketPair(std::array<int, 2>& fds);
    static void closePair(std::array<int, 2>& fds) noexcept;
    static void drainFd(int fd) noexcept;

    static std::array<Pipe, kSignalCount> pipes_;
    std::array<QSocketNotifier*, kSignalCount> notifiers_{};
};

}  // namespace Texxy
