//*****************************************************************************
//*****************************************************************************

#include "statdialog.h"
#include "blocknetapp.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

//*****************************************************************************
//*****************************************************************************
StatDialog::StatDialog(QWidget *parent)
    : QDialog(parent)
    , m_console(0)
{
    setupUi();
}

//*****************************************************************************
//*****************************************************************************
StatDialog::~StatDialog()
{

}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onLogMessage(const QString & msg)
{
    if (m_console)
    {
        m_console->append(msg);
    }
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::setupUi()
{
    QHBoxLayout * hbox = new QHBoxLayout;
    QVBoxLayout * vbox = new QVBoxLayout;

    QPushButton * generate = new QPushButton("generate", this);
    hbox->addWidget(generate);

    QPushButton * dump = new QPushButton("dump", this);
    hbox->addWidget(dump);

    hbox->addStretch();
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;

    m_searchText = new QLineEdit(this);
    hbox->addWidget(m_searchText);

    QPushButton * search = new QPushButton("search", this);
    hbox->addWidget(search);

    QPushButton * send = new QPushButton("send", this);
    hbox->addWidget(send);

    vbox->addLayout(hbox);

    m_console = new QTextEdit(this);
    vbox->addWidget(m_console);

    setLayout(vbox);

    BlocknetApp * app = qobject_cast<BlocknetApp *>(qApp);
    if (!app)
    {
        return;
    }

    connect(generate, SIGNAL(clicked()), app,  SLOT(onGenerate()));
    connect(dump,     SIGNAL(clicked()), app,  SLOT(onDump()));
    connect(search,   SIGNAL(clicked()), this, SLOT(onSearch()));
    connect(send,     SIGNAL(clicked()), this, SLOT(onSend()));
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onSearch()
{
    BlocknetApp * app = qobject_cast<BlocknetApp *>(qApp);
    if (!app)
    {
        return;
    }

    app->onSearch(m_searchText->text().toStdString());
}

//*****************************************************************************
//*****************************************************************************
void StatDialog::onSend()
{
    BlocknetApp * app = qobject_cast<BlocknetApp *>(qApp);
    if (!app)
    {
        return;
    }

    app->onSend(m_searchText->text().toStdString(), "test message");
}
