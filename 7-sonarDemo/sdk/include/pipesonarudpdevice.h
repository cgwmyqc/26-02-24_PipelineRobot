#ifndef PIPESONARUDPDEVICE_H
#define PIPESONARUDPDEVICE_H

#include <QObject>
#include "pipesonarparser.h"
#include "qudpdevice.h"

class PipeSonarUdpDevice : public QObject
{
    Q_OBJECT
public:
    explicit PipeSonarUdpDevice(QObject *parent = nullptr);
    ~PipeSonarUdpDevice();
    bool Init(QString ip, int port, QString localip,  int localport);    // 初始化函数
    void Close();
    int SendCommand(CONTROLDATA cmd);   // 发送数据
private:
    PipeSonarParser * sonarParser;
    QUdpDevice * udpDevice;
signals:
    void ReceiveSonarRelay(RETURNDATA relay);

public slots:

};

#endif // PIPESONARUDPDEVICE_H
