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

    static void hupSignalHandler(int);
    static void termSignalHandler(int);
    static void intSignalHandler(int);
    static void quitSignalHandler(int);

   signals:
    void sigQUIT();

   private slots:
    void handleSigHup();
    void handleSigTerm();
    void handleSigINT();
    void handleSigQUIT();

   private:
    [[nodiscard]] bool watchSignal(int sig);
    static bool makeSocketPair(std::array<int, 2>& fds);
    static void closePair(std::array<int, 2>& fds) noexcept;
    static void drainFd(int fd) noexcept;

    static inline std::array<int, 2> sighupFd{-1, -1};
    static inline std::array<int, 2> sigtermFd{-1, -1};
    static inline std::array<int, 2> sigintFd{-1, -1};
    static inline std::array<int, 2> sigquitFd{-1, -1};

    QSocketNotifier* snHup_ = nullptr;
    QSocketNotifier* snTerm_ = nullptr;
    QSocketNotifier* snInt_ = nullptr;
    QSocketNotifier* snQuit_ = nullptr;
};

}  // namespace Texxy
