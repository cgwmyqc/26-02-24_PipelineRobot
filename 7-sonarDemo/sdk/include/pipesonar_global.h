#ifndef PIPESONAR_GLOBAL_H
#define PIPESONAR_GLOBAL_H
#include <QObject>

// PipeSonar返回的一帧数据
typedef struct tagReturnData
{
    uchar HeadID;               // 0x10  0x20全部数据
    uchar TotalFrame;           // 总包数
    uchar CurrFrame;            // 当前包号
    double HeadAngle;           // 探头位置
    uchar StepDirection;        // 方向 0-顺时针 1-逆时针
    double HeadRange;           // 声呐扫描距离 25.0cm~6.0m
    quint16 NumberOfShots;      // 400 当前包的采样（轮廓）点数
    quint16 DataBytes;          // 800 采样（轮廓）的总点数
    double ProfileData[2100];   // 0x10是固定的, 0x20不一定
    quint8 t1;
    quint8 t2;
    quint8 t3;
    quint8 t4;
    quint8 t5;
    quint8 t6;
    quint8 t7;
    quint8 t8;
    quint64 time;                // 时间戳
    bool parse(QByteArray buf);
    QByteArray rawData;         // 原始数据
}RETURNDATA,*PRETURNDATA;

// 回放时读取的数据, 按照文件名记录数据段
typedef struct tagReplayData
{
    uchar HeadID;               // 0x10  0x20全部数据
    uchar TotalFrame;           // 总包数
    uchar CurrFrame;            // 当前包号
    double HeadAngle;           // 探头位置
    uchar StepDirection;        // 方向 0-顺时针 1-逆时针
    double HeadRange;           // 声呐扫描距离 25.0cm~6.0m
    quint16 NumberOfShots;      // 400 当前包的采样（轮廓）点数
    quint16 DataBytes;          // 800 采样（轮廓）的总点数
    quint64 StartPos;           // 0x10是固定的, 0x20不一定
    quint64 CurrLen;            // 当前帧长度
    quint8 t1;
    quint8 t2;
    quint8 t3;
    quint8 t4;
    quint8 t5;
    quint8 t6;
    quint8 t7;
    quint8 t8;
    quint64 time;                // 时间戳
    bool parse(QByteArray buf);
    QByteArray rawData;         // 原始数据
}REPLAYDATA,*PREPLAYDATA;

// PipeSonar存储一帧数据
typedef struct tagStorageFormat
{
    int nToReadIndex;
    int TotalBytes;
    int nToRead;
    QString Date;
    int ExtendedBytes;
    uchar Dir;              // 0-ccw 1-cw
    uchar Xdcr;             // 0-Dn  1-Up
    uchar Mode;             // 1-Polar
    uchar StepSize;         // 2=0.9Deg
    int StartGain;
    int SectorSize;
    int TrainAngle;
    double Absorption;
    uchar ProfileGrid;
    uchar Zero;
    uchar DataBits;
    uchar LOGF;
    int PulseLength;
    uchar Profile;
    double SoundVelocity;
    QString UserText;
    int OperatingFrequency;
    int VerticalAngleOffset;
    double CorrectedRange;
    RETURNDATA SonarData;
    QByteArray ToBytes();
}STORAGEFORMAT,*PSTORAGEFORMAT;

// 发送命令
typedef struct tagControlData
{
    uchar Power;            // 2021.03.11修改 10-50取值  2021.05.11修改 0-20取值
    uchar StopSign;         // 停止标记, 0:开始 1:停止
    double HeadRange;       // 25.0cm~6.0m
    uchar StepDirection;    // 方向 0-正常 1-反方向
    int StartGain;
    uchar LOGF;
    double Absorption;
    int PulseLength;
    double ProfileRange;
    double SwitchDelay;
    double Frequency;
    int DHCP;               // DHCP开关  0:OFF，使用静态IP模式  1:ON， 设备给PC端自动分配地址模式  0xFF:将IP配置恢复出厂设置
    int IPAddr[4];          // IP地址
    int MinAddr;            // 动态分配IP段的低地址
    int MaxAddr;            // 动态分配IP段的高地址
    qint64 time;            // 时间戳
    QByteArray ToBytes();
}CONTROLDATA,*PCONTROLDATA;

typedef struct tagSettingData
{
    QString comName;    // = inisetting->value(tr("COM/NAME"),"COM1").toString();
    int comBaud;        // = inisetting->value(tr("COM/BAUD"),"115200").toInt();
    QString ipAddr;     // = inisetting->value(tr("UDP/ADDR"),"127.0.0.1").toString();
    int ipPort;         // = inisetting->value(tr("UDP/PORT"),"10000").toInt();
    QString localAddr;  // = inisetting->value(tr("UDP/LOCALADDR"),"127.0.0.1").toString();
    int localPort;      // = inisetting->value(tr("UDP/LOCALPORT"),"10001").toInt();
    bool bSpDevice;     // = inisetting->value(tr("COM/SELECT"),"false").toBool();
    QString localAddr2; // = inisetting->value(tr("UDP/LOCALADDR2"),"127.0.0.1").toString();
    bool bDHCP;         // = inisetting->value(tr("UDP/DHCP"),"false").toString();
    QString actualAddr; // 不保存
    // 显示参数
    int drawSize;       // 显示数据点大小
    int drawMode;       // 显示模式
    int drawType;       // 显示类型
    float circleDiameter;// 参考圆直径

    float range;        // 量程
    int startgain;      // 强度
    int power;          // 功率

}SETTINTDATA,*PSETTINTDATA;

Q_DECLARE_METATYPE(CONTROLDATA)

#endif // PIPESONAR_GLOBAL_H
