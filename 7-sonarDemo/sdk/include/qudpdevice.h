#ifndef QUDPDEVICE_H
#define QUDPDEVICE_H

#include <QObject>
#include <QUdpSocket>
#include "qbasicdatastruct.h"

class QUdpDevice : public QObject
{
    Q_OBJECT
public:
    explicit QUdpDevice(QObject *parent = nullptr);
    bool Init(QString ip, int port, QString localip, int localport);            // 初始化 远程IP地址 远程端口号 本地端口号
    bool SendData(QByteArray array);            // 发送数据
    void Close();
private:
    QUdpSocket * udpSocket;         // 串口设备
    QString strServerAddr;          // 服务器地址
    int nServerPort;                // 服务器端口
signals:
    void UdpDataReceived(QBasicDataStruct);
public slots:

private slots:
    void onDataReadyRead();
};

#endif // QUDPDEVICE_H
