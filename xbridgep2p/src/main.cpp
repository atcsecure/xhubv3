//*****************************************************************************
//*****************************************************************************

#include "statdialog.h"
#include "xbridgeapp.h"

#include <QString>
// #include <QDateTime>
// #include <QThread>
#include <QDebug>
#include <QtGlobal>

//*****************************************************************************
//*****************************************************************************
void logOutput(QtMsgType type,
               const QMessageLogContext & /*context*/,
               const QString &msg)
{
    static bool recursion = false;
    if (recursion)
    {
        assert(false);
        return;
    }

    recursion = true;

//    QString logFileName = settings().logFileName();

    // QString mes = QString::fromUtf8(msg);
    // mes.replace('\n', ' ');

    // QString dt = QDateTime::currentDateTime().toString(QLatin1String("[dd.MM.yy hh:mm:ss] "));
    QString dt;// = QDateTime::currentDateTime().toString(QLatin1String("[hh:mm:ss] "));
    switch (type)
    {
        case QtDebugMsg:
            dt += QLatin1String("[D]");
            break;
        case QtWarningMsg:
            dt += QLatin1String("[W]");
            break;
        case QtCriticalMsg:
            dt += QLatin1String("[C]");
            break;
        case QtFatalMsg:
            dt += QLatin1String("[F]");
    }

//    dt += QString().sprintf(" [%04x]", QThread::currentThreadId());

//    QFile f(logFileName);

//    f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

//    QTextStream out(&f);

//    out << dt << QLatin1Char(' ') << mes << endl << flush;

//    f.close();

//    fprintf(stderr, "%s %s\n", dt.toLocal8Bit().constData(), msg.toLocal8Bit().constData());

    XBridgeApp * app = qobject_cast<XBridgeApp *>(qApp);
    if (app)
    {
        app->logMessage(msg);
    }

    recursion = false;
}

//*****************************************************************************
//*****************************************************************************
int main(int argc, char *argv[])
{
    XBridgeApp a(argc, argv);

    qInstallMessageHandler(logOutput);

    a.initDht();

    StatDialog w;
    a.connect(&a, SIGNAL(showLogMessage(const QString &)),
              &w, SLOT(onLogMessage(const QString &)));
    w.show();

    int retcode = a.exec();

    a.stopDht();

    return retcode;
}
