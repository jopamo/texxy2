// src/platform/singleton.cpp
/*
 singleton.cpp
*/

#include "singleton.h"
#include "texxyadaptor.h"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QDialog>
#include <QUrl>
#include <QRect>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>

#include <cstdio>
#include <utility>

#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <unistd.h>
#endif

#ifdef HAS_X11
#include "x11.h"
#endif

namespace Texxy {

static constexpr QLatin1StringView serviceName{"org.texxy.Texxy"};
static constexpr QLatin1StringView ifaceName{"org.texxy.Application"};

TexxyApplication::TexxyApplication(int& argc, char** argv) : QApplication(argc, argv) {
#ifdef HAS_X11
    const QString platform = QGuiApplication::platformName();
    isX11_ = (QString::compare(platform, QStringLiteral("xcb"), Qt::CaseInsensitive) == 0);
#else
    isX11_ = false;
#endif
    isWayland_ = false;

    isPrimaryInstance_ = true;
    standalone_ = false;
    quitSignalReceived_ = false;
    isRoot_ = false;

    config_.readConfig();
    lastFiles_ = config_.getLastFiles();

    if (config_.getSharedSearchHistory())
        searchModel_ = new QStandardItemModel(0, 1, this);
    else
        searchModel_ = nullptr;
}

TexxyApplication::~TexxyApplication() {
    qDeleteAll(Wins);
}

void TexxyApplication::init(bool standalone) {
    standalone_ = standalone;
    isPrimaryInstance_ = standalone;

    if (standalone_)
        return;

    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.isConnected()) {
        isPrimaryInstance_ = true;
        standalone_ = true;
        return;
    }

    if (dbus.registerService(QString::fromLatin1(serviceName.data(), serviceName.size()))) {
        isPrimaryInstance_ = true;
        new TexxyAdaptor(this);
        dbus.registerObject(QStringLiteral("/Application"), this);
    }
}

void TexxyApplication::quitting() {
    for (int i = 0; i < Wins.size(); ++i)
        Wins.at(i)->cleanUpOnTerminating(config_, i == Wins.size() - 1);

    delete searchModel_;
    searchModel_ = nullptr;

    config_.writeConfig();
}

void TexxyApplication::quitSignalReceived() {
    quitSignalReceived_ = true;
    quit();
}

void TexxyApplication::sendInfo(const QStringList& info) {
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.isConnected())
        return;

    QDBusInterface iface(QString::fromLatin1(serviceName.data(), serviceName.size()), QStringLiteral("/Application"),
                         QString::fromLatin1(ifaceName.data(), ifaceName.size()), dbus, this);
    if (iface.isValid())
        iface.call(QStringLiteral("handleInfo"), info);
}

void TexxyApplication::sendRecentFile(const QString& file, bool recentOpened) {
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.isConnected())
        return;

    QDBusMessage methodCall =
        QDBusMessage::createMethodCall(QString::fromLatin1(serviceName.data(), serviceName.size()),
                                       QStringLiteral("/Application"), QString(), QStringLiteral("addRecentFile"));
    methodCall.setAutoStartService(false);
    methodCall.setArguments(QList<QVariant>{file, recentOpened});
    dbus.call(methodCall, QDBus::NoBlock, 1000);
}

bool TexxyApplication::cursorInfo(QStringView cmdOpt, int& lineNum, int& posInLine) const {
    if (cmdOpt.isEmpty())
        return false;

    lineNum = 0;
    posInLine = 0;

    if (cmdOpt == u"+") {
        lineNum = -2;  // end of file marker used downstream
        posInLine = 0;
        return true;
    }

    if (cmdOpt.startsWith(u'+')) {
        bool ok = false;
        const QString s = cmdOpt.toString();  // safe because this path is rare
        lineNum = s.toInt(&ok);               // leading '+' is accepted by Qt
        if (ok) {
            if (lineNum > 0)
                lineNum += 1;  // 1 reserved for session files in newTabFromName
            return true;
        }

        const QStringList l = s.split(QLatin1Char(','));
        if (l.size() == 2) {
            lineNum = l.at(0).toInt(&ok);
            if (ok) {
                posInLine = l.at(1).toInt(&ok);
                if (ok) {
                    if (lineNum > 0)
                        lineNum += 1;
                    return true;
                }
            }
        }
    }
    return false;
}

QStringList TexxyApplication::processInfo(const QStringList& info,
                                          long& desktop,
                                          int& lineNum,
                                          int& posInLine,
                                          bool* newWindow) const {
    desktop = -1;
    lineNum = 0;
    posInLine = 0;

    QStringList sl = info;
    if (sl.isEmpty()) {
        *newWindow = true;
        return {};
    }

    *newWindow = standalone_;

    desktop = sl.first().toInt();
    sl.removeFirst();

    if (sl.isEmpty())
        return {};

    const QDir curDir(sl.first());
    sl.removeFirst();

    if (!sl.isEmpty() && (sl.first() == QLatin1String("--standalone") || sl.first() == QLatin1String("-s")))
        sl.removeFirst();

    if (sl.isEmpty())
        return {};

    bool hasCurInfo = cursorInfo(QStringView{sl.first()}, lineNum, posInLine);
    if (hasCurInfo) {
        sl.removeFirst();
        if (!sl.isEmpty()) {
            if (sl.first() == QLatin1String("--win") || sl.first() == QLatin1String("-w")) {
                *newWindow = true;
                sl.removeFirst();
            }
        }
    }
    else if (sl.first() == QLatin1String("--win") || sl.first() == QLatin1String("-w")) {
        *newWindow = true;
        sl.removeFirst();
        if (!sl.isEmpty())
            hasCurInfo = cursorInfo(QStringView{sl.first()}, lineNum, posInLine);
        if (hasCurInfo)
            sl.removeFirst();
    }

    QStringList filesList;
    filesList.reserve(sl.size());

    for (const QString& path : sl) {
        if (path.isEmpty())
            continue;

        QString realPath = path;

        const QUrl url{realPath};
        const QString scheme = url.scheme();

        if (scheme == QLatin1String("file"))
            realPath = url.toLocalFile();
        else if (scheme == QLatin1String("admin"))
            realPath = url.path();
        else if (!scheme.isEmpty())
            continue;

        realPath = curDir.absoluteFilePath(realPath);
        filesList << QDir::cleanPath(realPath);
    }

    if (filesList.isEmpty() && hasCurInfo)
        std::fputs("Texxy: file path/name is missing\n", stderr);

    return filesList;
}

void TexxyApplication::firstWin(const QStringList& info) {
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    isRoot_ = (geteuid() == 0);
#endif
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin = false;

    const QStringList filesList = processInfo(info, d, lineNum, posInLine, &openNewWin);

    newWin(filesList, lineNum, posInLine);

    lastFiles_.clear();
}

TexxyWindow* TexxyApplication::newWin(const QStringList& filesList, int lineNum, int posInLine) {
    auto* window = new TexxyWindow(nullptr);
    window->show();

    if (isRoot_)
        window->showRootWarning();

    Wins.append(window);

    if (!filesList.isEmpty()) {
        const bool multiple = (filesList.count() > 1 || window->isLoading());
        for (const QString& f : filesList)
            window->newTabFromName(f, lineNum, posInLine, multiple);
    }
    else if (!lastFiles_.isEmpty()) {
        const bool multiple = (lastFiles_.count() > 1 || window->isLoading());
        for (const QString& f : lastFiles_)
            window->newTabFromName(f, -1, 0, multiple);
    }

    return window;
}

void TexxyApplication::removeWin(TexxyWindow* win) {
    Wins.removeOne(win);
    win->deleteLater();
}

void TexxyApplication::handleInfo(const QStringList& info) {
    int lineNum = 0, posInLine = 0;
    long d = -1;
    bool openNewWin = false;

    const QStringList filesList = processInfo(info, d, lineNum, posInLine, &openNewWin);

    if (openNewWin) {
        newWin(filesList, lineNum, posInLine);
        return;
    }

    bool found = false;

    if (!config_.getOpenInWindows()) {
        QRect sr;
        if (QScreen* pScreen = QApplication::primaryScreen())
            sr = pScreen->virtualGeometry();

        for (int i = 0; i < Wins.count(); ++i) {
            TexxyWindow* thisWin = Wins.at(i);
#ifdef HAS_X11
            WId id = thisWin->winId();
            long whichDesktop = -1;
            if (isX11_)
                whichDesktop = onWhichDesktop(id);
#endif
            if (!isX11_
#ifdef HAS_X11
                || (whichDesktop == d || whichDesktop == -1)
#endif
            ) {
                bool hasDialog = thisWin->isLocked();
                if (!hasDialog) {
                    const QList<QDialog*> dialogs = thisWin->findChildren<QDialog*>();
                    for (QDialog* dlg : dialogs) {
                        if (dlg->isModal()) {
                            hasDialog = true;
                            break;
                        }
                    }
                }
                if (hasDialog)
                    continue;

                if (!isX11_ || sr.contains(thisWin->geometry().center())) {
                    if (d >= 0) {
                        thisWin->dummyWidget->showMinimized();
                        QTimer::singleShot(0, thisWin->dummyWidget, &QWidget::hide);
                    }

                    if (filesList.isEmpty())
                        thisWin->newTab();
                    else {
                        const bool multiple = (filesList.count() > 1 || thisWin->isLoading());
                        for (const QString& f : filesList)
                            thisWin->newTabFromName(f, lineNum, posInLine, multiple);
                    }
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found)
        newWin(filesList, lineNum, posInLine);
}

void TexxyApplication::addRecentFile(const QString& file, bool recentOpened) {
    if (config_.getRecentOpened() == recentOpened)
        config_.addRecentFile(file);
}

}  // namespace Texxy
