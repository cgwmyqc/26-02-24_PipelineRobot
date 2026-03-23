#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    as = new PipesonarSDK;


    as->init();

    if (as->ifinit)
    {
        qDebug()<<"[sonar] init done";
        as->connectSonar();
    }

    if (as->ifconnect)
    {
        qDebug()<<"[sonar] connect done";
    }
    
    testTimer = new QTimer(this);
    connect(testTimer, &QTimer::timeout, this, &MainWindow::onTestTimerout);
    testTimer->start(500);
    qDebug() << "testTimer started, interval = 500 ms";
    
}


void MainWindow::onTestTimerout()
{
    //QMap<double,QList<double>> angleList = as->sonarRada->angleList;
    QMap<double,double> angleDistance = as->sonarRada->angleDistance;
    qDebug()<<"list:"<<angleDistance;
}



MainWindow::~MainWindow()
{
    delete as;
    delete ui;
}

