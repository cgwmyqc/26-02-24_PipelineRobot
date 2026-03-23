#ifndef QSERIALPORTDEVICE_H
#define QSERIALPORTDEVICE_H

#include <QObject>
#include <QSerialPort>
#include "qbasicdatastruct.h"

class QSerialPortDevice : public QObject
{
    Q_OBJECT
public:
    explicit QSerialPortDevice(QObject *parent = nullptr);
    bool Init(QString com, int baud);          // 初始化 串口号 波特率
    bool SendData(QByteArray array);            // 发送数据
    void Close();
private:
    QSerialPort * serialPort;       // 串口设备

signals:
    void ComDataReceived(QBasicDataStruct);
public slots:

private slots:
    void onComReadyRead();
};

#endif // QSERIALPORTDEVICE_H
