/*
 texxy/singleton.cpp
*/

#include <QDir>
#include <QScreen>
#include <QDialog>
#include <QUrl>
#include <QDBusConnection>
#include <QDBusInterface>

#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
#include <unistd.h>  // for geteuid()
#endif

#include "singleton.h"
#include "texxyadaptor.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace Texxy {

static constexpr const char* serviceName = "org.texxy.Texxy";
static constexpr const char* ifaceName = "org.texxy.Application";

TexxyApplication::TexxyApplication(int& argc, char** argv) : QApplication(argc, argv) {
#ifdef HAS_X11
    const QString platform = QGuiApplication::platformName();
    isX11_ = (QString::compare(platform, QStringLiteral("xcb"), Qt::CaseInsensitive) == 0);
#else
    isX11_ = false;
#endif
    isWayland_ = false;  // explicitly do not support Wayland

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

    if (!standalone_) {
        QDBusConnection dbus = QDBusConnection::sessionBus();
        if (!dbus.isConnected()) {
            isPrimaryInstance_ = true;
            standalone_ = true;
        }
        else if (dbus.registerService(QLatin1String(serviceName))) {
            isPrimaryInstance_ = true;
            new TexxyAdaptor(this);
            dbus.registerObject(QStringLiteral("/Application"), this);
        }
    }
}

void TexxyApplication::quitting() {
    // save important info if windows aren't closed
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
    QDBusInterface iface(QLatin1String(serviceName), QStringLiteral("/Application"), QLatin1String(ifaceName), dbus,
                         this);
    iface.call(QStringLiteral("handleInfo"), info);
}

// called only in standalone mode
void TexxyApplication::sendRecentFile(const QString& file, bool recentOpened) {
    QDBusMessage methodCall = QDBusMessage::createMethodCall(QLatin1String(serviceName), QStringLiteral("/Application"),
                                                             QString(), QStringLiteral("addRecentFile"));
    methodCall.setAutoStartService(false);
    methodCall.setArguments(QList<QVariant>{file, recentOpened});
    QDBusConnection::sessionBus().call(methodCall, QDBus::NoBlock, 1000);
}

bool TexxyApplication::cursorInfo(const QString& commndOpt, int& lineNum, int& posInLine) {
    if (commndOpt.isEmpty())
        return false;

    lineNum = 0;  // no cursor placing
    posInLine = 0;

    if (commndOpt == QStringLiteral("+")) {
        lineNum = -2;  // means the end (-> TexxyWindow::newTabFromName)
        posInLine = 0;
        return true;
    }
    else if (commndOpt.startsWith(QLatin1Char('+'))) {
        bool ok = false;
        lineNum = commndOpt.toInt(&ok);  // "+" is included
        if (ok) {
            if (lineNum > 0)   // otherwise, the cursor will be ignored (-> TexxyWindow::newTabFromName)
                lineNum += 1;  // 1 is reserved for session files (-> TexxyWindow::newTabFromName)
            return true;
        }
        else {
            const QStringList l = commndOpt.split(QLatin1Char(','));
            if (l.count() == 2) {
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
    }
    return false;
}

QStringList TexxyApplication::processInfo(const QStringList& info,
                                     long& desktop,
                                     int& lineNum,
                                     int& posInLine,
                                     bool* newWindow) {
    desktop = -1;
    lineNum = 0;  // no cursor placing
    posInLine = 0;

    QStringList sl = info;
    if (sl.isEmpty()) {
        *newWindow = true;
        return {};
    }

    *newWindow = standalone_;  // standalone_ is true without D-Bus

    desktop = sl.at(0).toInt();
    sl.removeFirst();

    if (sl.isEmpty())
        return {};

    QDir curDir(sl.at(0));
    sl.removeFirst();

    if (!sl.isEmpty() && (sl.at(0) == QLatin1String("--standalone") || sl.at(0) == QLatin1String("-s")))
        sl.removeFirst();  // "--standalone" is always the first option

    if (sl.isEmpty())
        return {};

    bool hasCurInfo = cursorInfo(sl.at(0), lineNum, posInLine);
    if (hasCurInfo) {
        sl.removeFirst();
        if (!sl.isEmpty()) {
            if (sl.at(0) == QLatin1String("--win") || sl.at(0) == QLatin1String("-w")) {
                *newWindow = true;
                sl.removeFirst();
            }
        }
    }
    else if (sl.at(0) == QLatin1String("--win") || sl.at(0) == QLatin1String("-w")) {
        *newWindow = true;
        sl.removeFirst();
        if (!sl.isEmpty())
            hasCurInfo = cursorInfo(sl.at(0), lineNum, posInLine);
        if (hasCurInfo)
            sl.removeFirst();
    }

    // always return absolute clean paths
    QStringList filesList;
    filesList.reserve(sl.size());

#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
    for (const QString& path : std::as_const(sl))
#else
    for (const QString& path : qAsConst(sl))
#endif
    {
        if (path.isEmpty())
            continue;

        QString realPath = path;

        const QUrl url(realPath);
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
        qDebug("Texxy: File path/name is missing.");

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

    lastFiles_.clear();  // they should be called only with the session start
}

TexxyWindow* TexxyApplication::newWin(const QStringList& filesList, int lineNum, int posInLine) {
    TexxyWindow* window = new TexxyWindow(nullptr);
    window->show();

    if (isRoot_)
        window->showRootWarning();

    Wins.append(window);

    if (!filesList.isEmpty()) {
        const bool multiple = (filesList.count() > 1 || window->isLoading());
        for (int i = 0; i < filesList.count(); ++i)
            window->newTabFromName(filesList.at(i), lineNum, posInLine, multiple);
    }
    else if (!lastFiles_.isEmpty()) {
        const bool multiple = (lastFiles_.count() > 1 || window->isLoading());
        for (int i = 0; i < lastFiles_.count(); ++i)
            window->newTabFromName(lastFiles_.at(i), -1, 0, multiple);  // restore cursor positions too
    }

    return window;
}

void TexxyApplication::removeWin(TexxyWindow* win) {
    Wins.removeOne(win);
    win->deleteLater();
}

// called only by D-Bus
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
            // if the command is issued from where a Texxy window exists and window isn't minimized with a modal
            // dialog
            if (!isX11_
#ifdef HAS_X11
                || (whichDesktop == d || whichDesktop == -1)
#endif
            ) {
                bool hasDialog = thisWin->isLocked();
                if (!hasDialog) {
                    const QList<QDialog*> dialogs = thisWin->findChildren<QDialog*>();
                    for (int j = 0; j < dialogs.count(); ++j) {
                        if (dialogs.at(j)->isModal()) {
                            hasDialog = true;
                            break;
                        }
                    }
                }
                if (hasDialog)
                    continue;

                // consider viewports too and prefer a window inside the current viewport
                if (!isX11_ || sr.contains(thisWin->geometry().center())) {
                    if (d >= 0) {
                        // workaround for an old bug to keep desktops consistent
                        thisWin->dummyWidget->showMinimized();
                        QTimer::singleShot(0, thisWin->dummyWidget, &QWidget::hide);
                    }

                    if (filesList.isEmpty())
                        thisWin->newTab();
                    else {
                        const bool multiple = (filesList.count() > 1 || thisWin->isLoading());
                        for (int j = 0; j < filesList.count(); ++j)
                            thisWin->newTabFromName(filesList.at(j), lineNum, posInLine, multiple);
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

// called only by D-Bus
void TexxyApplication::addRecentFile(const QString& file, bool recentOpened) {
    if (config_.getRecentOpened() == recentOpened)
        config_.addRecentFile(file);
}

}  // namespace Texxy
