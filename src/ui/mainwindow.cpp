//******************************************************************************
//******************************************************************************

#include "mainwindow.h"
#include "xbridgetransactionsview.h"
#include "../xbridgeapp.h"
#include "../xbridgeexchange.h"

#include <QStatusBar>

//******************************************************************************
//******************************************************************************
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setMinimumSize(QSize(800, 640));

    QString title = tr("Blocknet Decentralized Exchange v.%1")
            .arg(QString::fromStdString(XBridgeApp::version()));
    if (XBridgeExchange::instance().isEnabled())
    {
        title += tr(" [exhange enabled]");
    }
    setWindowTitle(title);

    setCentralWidget(new XBridgeTransactionsView(this));

    statusBar()->addWidget(&m_peers);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    onTimer();

    m_timer.start(5000);
}

//******************************************************************************
//******************************************************************************
MainWindow::~MainWindow()
{

}

//******************************************************************************
//******************************************************************************
void MainWindow::onTimer()
{
    XBridgeApp & app = XBridgeApp::instance();
    m_peers.setText(tr("Peers: <%1>").arg(QString::number(app.peersCount())));
}
