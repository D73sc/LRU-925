#ifndef DDR6ROBOTWIDGET_H
#define DDR6ROBOTWIDGET_H

#include "STLModel/rrglwidget.h"
#include "STLModel/stlfileloader.h"
#include <QVector>
//继承RRGLWidget,绘制取盖板模型
// 取盖板结构（4 个部件）
typedef struct {
    STLFileLoader *link0;
    STLFileLoader *link1;
    STLFileLoader *link2;
    STLFileLoader *link3;
    STLFileLoader *link4;
} CoverPlateSTLModel;

// 桌子模型结构
typedef struct {
    STLFileLoader *link0;
} DeskModel;

// 配置参数
typedef struct {
    QVector<float> x;      // 沿 x 轴平移
    QVector<float> y;      // 沿 y 轴平移
    QVector<float> z;      // 沿 z 轴平移
} RobotConfig;

// 全局显示配置
typedef struct {
    bool isDrawGrid;
    bool isDrawWorldCoord;
    bool isDrawJoint1Coord;
    bool isDrawJoint2Coord;
    bool isDrawJoint3Coord;
    bool isDrawJoint4Coord;
} GlobalConfig;

class DDR6RobotWidget : public RRGLWidget {
    Q_OBJECT

public:
    explicit DDR6RobotWidget(QWidget *parent = nullptr);
    ~DDR6RobotWidget();

    // 加载取盖板 STL 模型
    void loadCoverPlateModelSTLFile();

    // 配置模型参数
    void configureModelParams();

    // 公开配置供外部修改
    RobotConfig mRobotConfig;
    GlobalConfig mGlobalConfig;

protected:
    void drawGL() override;

private:
    CoverPlateSTLModel mRobotModel;
};

#endif // DDR6ROBOTWIDGET_H
