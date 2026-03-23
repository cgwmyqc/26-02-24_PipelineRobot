#ifndef PIPERSONARREPLAY_H
#define PIPERSONARREPLAY_H

#include <QObject>
#include "pipesonar_global.h"
// 声呐回放类, 读取回放数据结构, 回访时才读取数据
class PipeSonarReplay : public QObject
{
    Q_OBJECT
public:
    explicit PipeSonarReplay(QObject *parent = nullptr);
    int LoadDataFromFile(QString filename, bool &bok);    // 读取文件处理成回放列表
    void Replay(int pos);
signals:
    void ReplaySonarRelay(RETURNDATA relay);
private:
    RETURNDATA dispdata;
    QString replayFile;                                     // 回放文件名
    QList<QList<REPLAYDATA>> lstReplay;                     // 回放列表
    QList<QList<REPLAYDATA>> ParseData(const QByteArray & array);  // 读取文件处理成链表
};

#endif // PIPERSONARREPLAY_H
