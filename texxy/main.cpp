/*
 * texxy/main.cpp
 */

#include <QDir>
#include <QTextStream>
#include "singleton.h"
#include "signalDaemon.h"

#ifdef HAS_X11
#include "x11.h"
#endif

int main(int argc, char** argv) {
    const QString name = "Texxy";
    const QString version = "0.9.1";

    Texxy::TexxyApplication singleton(argc, argv);
    singleton.setApplicationName(name);
    singleton.setApplicationVersion(version);

    QStringList args = singleton.arguments();
    if (!args.isEmpty())
        args.removeFirst();

    QString firstArg;
    if (!args.isEmpty())
        firstArg = args.at(0);

    if (firstArg == "--help" || firstArg == "-h") {
        QTextStream out(stdout);
        out << "Texxy - Lightweight Qt text editor\n"
               "Usage:\n	texxy [option(s)] [file1 file2 ...]\n\n"
               "Options:\n\n"
               "--help or -h        Show this help and exit.\n"
               "--version or -v     Show version information and exit.\n"
               "--standalone or -s  Start a standalone process of Texxy.\n"
               "--win or -w         Open file(s) in a new window.\n"
               "+                   Place cursor at document end.\n"
               "+<L>                Place cursor at start of line L (L starts from 1).\n"
               "+<L>,<P>            Place cursor at position P of line L (P starts from 0\n"
               "                    but a negative value means line end).\n"
               "\nNOTE1: <X> means number X without brackets.\n"
               "NOTE2: --standalone or -s can only be the first option. If it exists,\n"
               "       --win or -w will be ignored because a standalone process always\n"
               "       has its separate, single window.\n"
               "NOTE3: --win or -w can come before or after cursor option, with a space\n"
               "       in between."
            << Qt::endl;
        return 0;
    }
    else if (firstArg == "--version" || firstArg == "-v") {
        QTextStream out(stdout);
        out << name << " " << version << Qt::endl;
        return 0;
    }

    singleton.init(firstArg == "--standalone" || firstArg == "-s");

    // translations support removed

    QStringList info;
#ifdef HAS_X11
    int d = singleton.isX11() ? static_cast<int>(Texxy::fromDesktop()) : -1;
#else
    int d = -1;
#endif
    info << QString::number(d) << QDir::currentPath();
    if (!args.isEmpty())
        info << args;

    if (!singleton.isPrimaryInstance()) {
        singleton.sendInfo(info);  // sent to the primary instance
        return 0;
    }

    // Handle SIGQUIT, SIGINT, SIGTERM and SIGHUP -> https://en.wikipedia.org/wiki/Unix_signal
    Texxy::signalDaemon D;
    D.watchUnixSignals();
    QObject::connect(&D, &Texxy::signalDaemon::sigQUIT, &singleton, &Texxy::TexxyApplication::quitSignalReceived);

    QObject::connect(&singleton, &QCoreApplication::aboutToQuit, &singleton, &Texxy::TexxyApplication::quitting);
    singleton.firstWin(info);

    return singleton.exec();
}
