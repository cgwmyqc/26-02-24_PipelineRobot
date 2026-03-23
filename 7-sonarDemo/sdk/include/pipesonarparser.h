#ifndef PIPESONARPARSER_H
#define PIPESONARPARSER_H

#include <QObject>
#include <QList>
#include <QThread>
#include <QMutexLocker>
#include <QMutex>
#include "qbasicdatastruct.h"
#include "pipesonar_global.h"

class PipeSonarParser : public QThread
{
    Q_OBJECT
public:
    explicit PipeSonarParser(QObject *parent = nullptr);
    QByteArray GenerateDataArray(uchar Addr, uchar Command, uint16_t Start, uint16_t Size);
    void SetStopSign() {bStop=true;}
    int GetBufferCount() {QMutexLocker locker(&mutex);return listDataArray.count();}
protected:
    void run();

private:
    mutable QMutex mutex;
    bool  bStop;            // 线程循环控制
    void ExecuteParse(QBasicDataStruct data);
    QBasicDataStruct lastRecvData;          // 上一次接收数据
    int comDataLen;                         // 处理本帧长度
    QByteArray inBuffer;                    // 解析用接收缓冲区
    QList<QBasicDataStruct> listDataArray;  // 接收原始数据列表
    CONTROLDATA lastSndCommand;             // 上一次发送命令
    RETURNDATA lastRevRelay;                // 上一次接收命令
signals:
    void ReceiveSonarRelay(RETURNDATA relay);

public slots:
    void ReceiveDataArray(QBasicDataStruct data);
};

#endif // PIPESONARPARSER_H
