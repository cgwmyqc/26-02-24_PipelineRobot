#ifndef PIPESONARDEVICE_H
#define PIPESONARDEVICE_H

#include <QObject>
#include "qserialportdevice.h"
#include "pipesonarparser.h"

class PipeSonarDevice : public QObject
{
    Q_OBJECT
public:
    explicit PipeSonarDevice(QObject *parent = nullptr);
    ~PipeSonarDevice();
    bool Init(QString com, int baud);      // 初始化函数
    void Close();
    int SendCommand(CONTROLDATA cmd);   // 发送数据
private:
    PipeSonarParser * sonarParser;
    QSerialPortDevice * spDevice;
signals:
    void ReceiveSonarRelay(RETURNDATA relay);

public slots:
};

#endif // PIPESONARDEVICE_H
