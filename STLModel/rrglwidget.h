#ifndef RRGLWIDGET_H
#define RRGLWIDGET_H

#include <QGLWidget>
#include <QMouseEvent>
#include <QPoint>
//鼠标旋转、缩小、平移、网格、坐标系
class RRGLWidget : public QGLWidget {
    Q_OBJECT

public:
    explicit RRGLWidget(QWidget *parent = nullptr);
    ~RRGLWidget();

    // 设置旋转角度
    void setXRotation(double angle);
    void setYRotation(double angle);
    void setZRotation(double angle);

    // 缩放
    void setScaleFactor(double factor);

signals:


protected:
    // 重写 QGLWidget 方法
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override; //重置按键状态

    // 绘制辅助函数
    void drawGrid();
    void drawCoordinates();
    void drawSTLCoordinates(int r, int g, int b);
    void setupColor(int r, int g, int b);

    // 纯虚函数，由子类实现具体绘制
    virtual void drawGL() = 0;

protected:
    // 旋转角度
    double xRot;
    double  yRot;
    double  zRot;

    // 缩放因子
    double scaleFactor;

    // 鼠标状态
    QPoint lastMousePos;       // 上一帧鼠标位置
    Qt::MouseButton curButton; // 当前按下的鼠标按钮
    bool isMouseDragging;      // 是否正在拖拽

private:
    // 设置颜色
    void setupColor(float r, float g, float b);
    // 角度归一化
    double normalizeAngle(double angle, double min, double max);
};

#endif // RRGLWIDGET_H
