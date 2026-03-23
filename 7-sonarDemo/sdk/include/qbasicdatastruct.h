#ifndef QBASICDATASTRUCT_H
#define QBASICDATASTRUCT_H

#include <QObject>
#include <QDateTime>
#include <QByteArray>

class QBasicDataStruct
{
public:
    QBasicDataStruct();
    QBasicDataStruct(QByteArray data,QDateTime time = QDateTime::currentDateTime());
    qint64 GetTimeInterval(QDateTime time);
    QDateTime GetTime() {return timeData;}
    QByteArray GetData(){return arrayData;}
private:
    QDateTime timeData;     // 命令接收时间
    QByteArray arrayData;   // 命令数据
};

Q_DECLARE_METATYPE(QBasicDataStruct)
#endif // QBASICDATASTRUCT_H
