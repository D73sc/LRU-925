#include "STLModel/coverplatewidget.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

//继承RRGLWidget,绘制取盖板模型
DDR6RobotWidget::DDR6RobotWidget(QWidget *parent)
    : RRGLWidget(parent)
{
    // 初始化配置
    configureModelParams();

    // 默认显示配置
    mGlobalConfig = {true, false, false, false, false, false};

    // 加载模型
    loadCoverPlateModelSTLFile();
}

DDR6RobotWidget::~DDR6RobotWidget()
{
    delete mRobotModel.link0;
    delete mRobotModel.link1;
    delete mRobotModel.link2;
    delete mRobotModel.link3;
    delete mRobotModel.link4;
}

void DDR6RobotWidget::loadCoverPlateModelSTLFile()
{
    QString appPath = QCoreApplication::applicationDirPath();
    QString stlDir = appPath + "/STL/";

    // 模型由 5 个小部件组成
    mRobotModel.link0 = new STLFileLoader(stlDir + "0.STL", 0.5);
    mRobotModel.link1 = new STLFileLoader(stlDir + "1.STL", 0.5);
    mRobotModel.link2 = new STLFileLoader(stlDir + "2.STL", 0.5);
    mRobotModel.link3 = new STLFileLoader(stlDir + "3.STL", 0.5);
    mRobotModel.link4 = new STLFileLoader(stlDir + "4.STL", 0.5);
}

void DDR6RobotWidget::configureModelParams()
{
    // 运动参数配置 5部件
    mRobotConfig.x = {-550, -650, -495, -115 ,-115};
    mRobotConfig.y = {0, 80, 20, 40 ,40};
    mRobotConfig.z = {-550, 35, 195, 100 , 340};
}

void DDR6RobotWidget::drawGL()
{
    glPushMatrix();

    // 绘制网格和坐标系
    if (mGlobalConfig.isDrawGrid) {
        drawGrid();
    }
    if (mGlobalConfig.isDrawWorldCoord) {
        drawCoordinates();
    }

    // 横移机构（link0）
    if (mGlobalConfig.isDrawJoint1Coord) {
        drawSTLCoordinates(255, 0, 0);
    }
    setupColor(0, 180, 90);
    glRotatef(90, 1.0, 0.0, 0.0); // 绕x轴旋转90
    glTranslatef(mRobotConfig.x[0], 0.0, 0.0);
    glTranslatef(0.0, mRobotConfig.y[0], 0.0);
    glTranslatef(0.0, 0.0, mRobotConfig.z[0]);
    mRobotModel.link0->draw();

    // 纵移机构（link1）
    if (mGlobalConfig.isDrawJoint2Coord) {
        drawSTLCoordinates(0, 122, 255);
    }
    setupColor(0, 122, 255);
    glRotatef(90, 0.0, 1.0, 0.0); //绕Y轴旋转90
    glTranslatef(mRobotConfig.x[1], 0.0, 0.0);
    glTranslatef(0.0, mRobotConfig.y[1], 0.0);
    glTranslatef(0.0, 0.0, mRobotConfig.z[1]);//Z正方向为横移正方向
    mRobotModel.link1->draw();


    //托举机构（link2）
    if (mGlobalConfig.isDrawJoint3Coord) {
        drawSTLCoordinates(0, 0, 255);
    }
    setupColor(255, 190, 0);
    glRotatef(90, 0.0, 1.0, 0.0); //绕Y轴旋转90
    glTranslatef(mRobotConfig.x[2], 0.0, 0.0);
    glTranslatef(0.0, mRobotConfig.y[2], 0.0);//Y正方向为托举正方向
    glTranslatef(0.0, 0.0, mRobotConfig.z[2]);//Z正方向为纵移正方向
    mRobotModel.link2->draw();


    // 夹爪机构1（link3）
    if (mGlobalConfig.isDrawJoint4Coord) {
        drawSTLCoordinates(255, 255, 0);
    }
    setupColor(200, 200, 200);
    glPushMatrix();  // 压栈保存变换前矩阵
    glRotatef(90, 0.0, 1.0, 0.0); //绕Y轴旋转90
    glTranslatef(mRobotConfig.x[3], 0.0, 0.0);
    glTranslatef(0.0, mRobotConfig.y[3], 0.0);
    glTranslatef(0.0, 0.0, mRobotConfig.z[3]);//-Z为手指运动正方向
    mRobotModel.link3->draw();
    glPopMatrix();   //弹栈



    // 夹爪机构2（link4）
    setupColor(200, 200, 200);
    glRotatef(90, 0.0, 1.0, 0.0); //绕Y轴旋转90
    glTranslatef(mRobotConfig.x[4], 0.0, 0.0);
    glTranslatef(0.0, mRobotConfig.y[4], 0.0);
    glTranslatef(0.0, 0.0, mRobotConfig.z[4]);//-Z为手指运动正方向
    mRobotModel.link4->draw();

    glPopMatrix();
}
