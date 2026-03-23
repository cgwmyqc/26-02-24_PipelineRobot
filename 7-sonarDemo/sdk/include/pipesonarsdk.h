#ifndef PIPESONARSDK_H
#define PIPESONARSDK_H

#include <QObject>
#include "PipesonarSDK_global.h"
#include "pipesonar_global.h"
#include "pipesonarwidget.h"
#include <QThread>
#include "pipesonardevice.h"
#include "pipesonarudpdevice.h"
#include "pipesonarreplay.h"
#include "pipesonarxyz.h"

static void getGateway( QString &activeIP, QString &gateway, const QString &wantIP);

class PIPESONARSDK_EXPORT PipesonarSDK : public QObject
{
    Q_OBJECT
private:
    std::vector<std::string> split(const std::string& str, const std::string& pattern);

    bool ConnectDevice();
    void DisConnectDevice();

private slots:
    void ReceiveSonarRelay(RETURNDATA relay);

protected:
    void timerEvent(QTimerEvent *event);
    void resizeEvent(QResizeEvent * event);

public:
    explicit PipesonarSDK(QObject *parent = nullptr);
    ~PipesonarSDK();

    SETTINTDATA setting;    // 设置参数
    CONTROLDATA ctrCmd;     // 控制变量

    PipeSonarDevice device; // 声呐串口设备
    PipeSonarUdpDevice updDevice;     // 声纳udp设备

    int nDispTotal;         // 回放总数
    int nDispOrder;         // 当前回放序号
    int nTimer;             // 回放定时器
    int nSpeedRatio;        // 快放系数

    int ConnTimer;          // 连接时定时器
    int TimerCount;         // 连接定时计数
    bool bReceived;         // 收到数据

    PipeSonarReplay control;       // 回放控制
    pipesonarxyz sonarxyz;         // 转为xyz

    PipeSonarWidget * sonarRada; // 雷达图像

    bool ifinit;                // 初始化判断位
    bool ifdhcp;                // dhcp判断位
    bool ifconnect;             // 连接标志位

    bool ifReceived;         // 收到数据标志位
    bool ifmeasure;          // 测量标志位
    bool ifsave;             // 保存标志位
    bool ifreplay;           // 回放标志位

    float Range;                // 声纳量程
    int StartGain;              // 声纳强度
    int Power;                  // 声纳功率

    int DrawSize;               // 图形点大小
    // 显示模式 0:无 1:水平翻转 2:垂直翻转 3:顺时针旋转90度 4:顺时针旋转180度 5:顺时针旋转270度
    int DrawMode;               // 图形绘制模式
    int DrawType;               // 图形类别
    float CircleDiameter;         // 参考圆直径

    // 初始化
    bool init();                // 初始化

    // 连接声纳
    bool connectSonar();        // 连接声纳
    bool disconnectSonar();     // 断开连接

    bool sendCommand();         // 发送指令

    // 量程
    bool setRange(float range);   // 设置声纳量程
    float getRange();             // 获取声纳量程
    bool addRange();              // 增大一档量程
    bool downRange();             // 减小一档量程

    // 强度
    bool setGain(int gain);       // 设置声纳强度
    int getGain();                // 获取声纳强度
    bool addGain();               // 增大一档强度
    bool downGain();              // 减小一档强度

    // 功率
    bool setPower(int power);     // 设置声纳功率
    int getPower();               // 获取声纳功率

    // 保存
    bool startSave();             // 开始保存文件
    bool stopSave();              // 停止保存文件

    // 回放
    bool startReplay();            // 开始回放
    bool pauseReplay();            // 暂停回放
    bool continueReplay();         // 继续回放
    bool stopReplay();             // 结束回放
    bool setReplaySpeed(int speed);// 设置回放速度
    int getReplaySpeed();          // 获取回放速度
    bool setReplayPositin(int positon);// 设置回放位置
    int getReplayPosition();       // 获取回放位置
    int getTotalPosition();        // 获取总位置长度
    quint64 getReplayTimeStamp();  // 获取回放时间戳
    QString getReplayTimeString(); // 获取回放时间字符串

    // 图形
    bool drawRada();
    bool zoomIn();                 // 图形放大
    bool zoomOut();                // 图形缩小

    bool setPointSize(int size);   // 设置显示点大小
    int getPointSize();            // 获取显示点大小

    bool setCircleDiameter(float value); // 设置参考圆直径
    float getCircleDiameter();           // 获取参考圆直径

    bool setPicMode(int mode);         // 设置图像模式：0：无，1-水平翻转，2-垂直翻转，3-顺时针旋转90度，4-顺时针旋转180度，5顺时针旋转270度
    int getPicMode();                  // 获取图像模式

    bool setPicType(int type);         // 设置图像类别：1-轮廓，0-实时
    int getPicType();                  // 获取图像类别

    // 测量
    bool startMeasure();               // 开始测量
    bool stopMeasure();                // 停止测量
    float getMeasureData();           // 获取测量数据

    // 网络
    int DhcpMode;                                     // DHCP模式 0：无dhcp 1：dhcp
    bool setDhcpMode(int dhcpmode);                   // 设置dhcp模式
    int getDhcpMode();                                // 获取dhcp模式
    bool setIP(QString sonarIp, QString computerIp);  // 设置上下位机模式
    QString getComputerIP();                          // 获取本机IP
    QString getSonarIP();                             // 获取声纳IP

    // 转XYZ
    bool transToXYZ();                             // 将声纳回放数据转为XYZ格式

};

#endif // PIPESONARSDK_H
