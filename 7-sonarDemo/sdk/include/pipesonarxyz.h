#ifndef PIPESONARXYZ_H
#define PIPESONARXYZ_H

#include <QObject>
#include "pipesonar_global.h"

class pipesonarxyz: public QObject
{
    Q_OBJECT
public:
    explicit pipesonarxyz(QObject *parent = nullptr);
    int LoadDataFromFile(QString filename);
    void Replay(int pos);

    void transformXYZ(QString filename);

private:
    RETURNDATA dispdata;
    QString replayFile;                                     // 转换文件名
    QList<QList<REPLAYDATA>> lstReplay;                     // 转换列表
    QList<QList<REPLAYDATA>> ParseData(const QByteArray & array);  // 读取文件处理成链表

};

#endif // PIPESONARXYZ_H
