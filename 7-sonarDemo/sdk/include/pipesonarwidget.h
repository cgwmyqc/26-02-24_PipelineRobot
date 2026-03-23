#ifndef PIPESONARWIDGET_H
#define PIPESONARWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QMouseEvent>
#include "pipesonar_global.h"

union myrgb
{
    uint32_t rgba;
    uchar rgba_bits[4];
};

class PipeSonarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PipeSonarWidget(QWidget *parent = nullptr);

    float measureDistance;

    void rawTime2Str(quint64 time);

    QMap<double,QList<double>> angleList;
    double headAngle;           // 数据角度（位置）

    QMap<double,double> angleDistance;
    double distanceAngle;


    // 时间戳
    quint64 timeStamp;
    // 显示时间信息
    QString strTime;
    void InitView();
    void SetSonarData(const RETURNDATA &data);
    void SetSonarData(const CONTROLDATA &ctrl, const RETURNDATA &data);
    void SetControlData(const CONTROLDATA &cmd);
    void SetWarnning(bool warn) {bWarn = warn;}
    bool IsPlayback() {return bHasData;}
    void SetStateString(QString str) {strState = str;}
    void SetTimeString(QString str) {strTime = str;}
    void SetWarnString(QString str) {strWarn = str;}
    void SaveFile(QString str);
    void StopSaveFile() {bSave=false;}              // 设置停止记录文件标记
    void Zoomin();                                  // 放大
    void Zoomout();                                 // 缩小
    void SetDrawPointSize(int size);                // 设置绘制的数据点大小
    void StartMearsure() {bRuler=true;nPointNum=0;} // 开始尺寸测量
    void StopMearsure() {bRuler=false;}             // 结束尺寸测量
    void SetDrawMode(int mode);                     // 设置自定义显示模式
    void SetDrawType(bool type);                    // 设置显示图像类型
    void SetCircleRadius(double r);                 // 设置圆半径
protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
signals:
    void ShowPointParam(double x, double y, double r, double a);

public slots:

private:
    // 绘制轮廓图
    void DrawProfileFigure(QPainter* painter);
    // 显示雷达图
    void DrawRadaFigure(QPainter * painter);
    // 显示雷达图
    void DrawBackground(QPainter * painter);
    QPoint MapToWidget(const QPoint & pos);     // 将实测值转换为屏幕坐标值
    QPointF MapToWidget(const QPointF & pos);   // 将实测值转换为屏幕坐标值
    QPointF MapToActual(const QPoint & pos);    // 将屏幕坐标值转换为实测值
    // 调色板调色
    uint32_t GetColorPallete(uchar grey, int mode);
    void SetRadaLine(const RETURNDATA &data, QPainter * painter, bool bDisp);

private:
    bool bProfileFigure;        // true显示轮廓图 false显示雷达图
    ////显示雷达图用变量
    double dRadaAngle;          // 雷达线
    double dPreAngle;           // 之前雷达线
    QMap<double, RETURNDATA> mapRadaData;   // 保存雷达数据
    QMap<int, RETURNDATA> mapProfileData;   // 保存轮廓数据
    RETURNDATA radaLine;        // 雷达线
    int nColorPallete;          // 颜色模板
    QImage *imgRada;            // 雷达图形

    // 显示模式 0:无 1:水平翻转 2:垂直翻转 3:顺时针旋转90度 4:逆时针旋转90度
    int drawMode;

    // 是否初始化
    bool bInit;
    // 中心点坐标
    int center_x;
    int center_y;
    // 缩放比例系数
    double zoom_ratio;
    // 显示比例系数
    double zoom_x;
    double zoom_y;
    // 坐标格实际长度
    double actual_x;
    double actual_y;
    // 坐标格屏幕长度
    int screen_x;
    int screen_y;
    // 圆直径
    double circle_r;
    // 绘制点大小
    int nDrawPoint;         // 绘制点大小
    // 绘制数据
    RETURNDATA aShot;    // 回放时用这个结构
    bool bHasData;      // true:回放 false:直接连接
    CONTROLDATA aCmd;   // 直接连接时用这个
    // 状态信息
    QString strState;

//    qint64 rawtime;

    // 标尺量算
    bool bRuler;        // 标尺量算标记
    bool bRulerline;    // 标尺画线标记
    QPoint startpt;     // 标尺开始点
    QPoint stoppt;      // 标尺结束点
    int nPointNum;      // 标尺点数

    // 显示警告
    bool bWarn;
    bool bWarnBlink;
    QString strWarn;    // 警告信息
    bool bRefresh;      // 是否清除显示

    // 是否保存
    bool bSave;             // 是否保存
    QString strFileName;    // 文件名称
};

#endif // PIPESONARWIDGET_H
