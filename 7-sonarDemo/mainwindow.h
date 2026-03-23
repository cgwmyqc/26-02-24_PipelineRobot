#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "pipesonarsdk.h"
#include "PipesonarSDK_global.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    PipesonarSDK *as;


private:
    Ui::MainWindow *ui;
    QTimer *testTimer;

    void onTestTimerout();

};
#endif // MAINWINDOW_H
