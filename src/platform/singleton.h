#pragma once

#include <QApplication>
#include <QStandardItemModel>
#include "texxywindow.h"
#include "config.h"

namespace Texxy {

// single-instance control using the DBus session bus
class TexxyApplication : public QApplication {
    Q_OBJECT
   public:
    TexxyApplication(int& argc, char** argv);
    ~TexxyApplication() override;

    void init(bool standalone);

    void sendInfo(const QStringList& info);
    void handleInfo(const QStringList& info);

    void sendRecentFile(const QString& file, bool recentOpened);
    void addRecentFile(const QString& file, bool recentOpened);

    void firstWin(const QStringList& info);
    TexxyWindow* newWin(const QStringList& filesList = {}, int lineNum = 0, int posInLine = 0);
    void removeWin(TexxyWindow* win);

    QList<TexxyWindow*> Wins;

    Config& getConfig() { return config_; }

    bool isPrimaryInstance() const { return isPrimaryInstance_; }
    bool isStandAlone() const { return standalone_; }
    bool isX11() const { return isX11_; }
    bool isWayland() const { return isWayland_; }
    bool isRoot() const { return isRoot_; }
    bool isQuitSignalReceived() const { return quitSignalReceived_; }

    QStandardItemModel* searchModel() const { return searchModel_; }

   public slots:
    void quitSignalReceived();
    void quitting();

   private:
    [[nodiscard]] bool cursorInfo(QStringView cmdOpt, int& lineNum, int& posInLine) const;
    [[nodiscard]] QStringList processInfo(const QStringList& info,
                                          long& desktop,
                                          int& lineNum,
                                          int& posInLine,
                                          bool* newWindow) const;

    bool quitSignalReceived_ = false;
    Config config_;
    QStringList lastFiles_;
    bool isPrimaryInstance_ = true;
    bool standalone_ = false;
    bool isX11_ = false;
    bool isWayland_ = false;
    bool isRoot_ = false;
    QStandardItemModel* searchModel_ = nullptr;
};

}  // namespace Texxy
