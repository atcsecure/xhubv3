//******************************************************************************
//******************************************************************************

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QLabel>
#include <QTimer>

//******************************************************************************
//******************************************************************************
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:

public slots:

private slots:
    void onTimer();

private:
    QLabel m_peers;
    QTimer m_timer;
};

#endif // MAINWINDOW_H
