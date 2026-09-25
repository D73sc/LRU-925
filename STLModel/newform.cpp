#include "STLModel/newform.h"
#include "ui_newform.h"
#include <QScrollArea>

NewForm::NewForm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::NewForm)
{
    ui->setupUi(this);
    // 所有 QCheckBox，自动连接信号
    bindAllCheckBoxes();
    zm = ZMotionControl::getinstance();
    UpdateTimer = new QTimer(this);
    connect(UpdateTimer,&QTimer::timeout,this,&NewForm::on_UpdateTimer_Out);
    UpdateTimer->start(200);
}


static int mapValue(float value, float inMin, float inMax, int outMin, int outMax) {

    float t = (value - inMin) / (inMax - inMin);
    int result = outMin + static_cast<int>(t * (outMax - outMin));
    return result;
}

void NewForm::on_UpdateTimer_Out()
{
    float pos1 = zm->GetAxisDpos(5);  // 纵移
    float pos2 = zm->GetAxisDpos(7);  // 横移
    float pos3 = zm->GetAxisDpos(9);  // 手指
    float pos4 = zm->GetAxisDpos(11); // 托举

    const float eps = 0.1f;

    // 更新横移
    if (qAbs(pos2 - m_lastPos2) > eps) {
        m_lastPos2 = pos2;
        int sliderVal = mapValue(pos2, 0.0f, 1134.0f, 35, 650);
        ui->sliderJoint1->setValue(sliderVal);
    }

    // 更新纵移
    if (qAbs(pos1 - m_lastPos1) > eps) {
        m_lastPos1 = pos1;
        int sliderVal = mapValue(pos1, -330.0f, 330.0f, 50, 350);
        ui->sliderJoint2->setValue(sliderVal);
    }

    //  更新托举
    if (qAbs(pos4 - m_lastPos4) > eps) {
        m_lastPos4 = pos4;
        int sliderVal = mapValue(pos4, 55.0f, 0.0f, 0, 40);
        ui->sliderJoint3->setValue(sliderVal);
    }

    // 更新夹爪
    if (qAbs(pos3 - m_lastPos3) > eps) {
        m_lastPos3 = pos3;
        int sliderVal = mapValue(pos3, -25.0f, -210.0f, 100, 200);
        ui->sliderJoint4->setValue(sliderVal);
    }
}


void NewForm::bindAllCheckBoxes()
{
    QList<QCheckBox*> checkBoxes = this->findChildren<QCheckBox*>();
    foreach(QCheckBox* box, checkBoxes)
    {
        connect(box, &QCheckBox::stateChanged, this, &NewForm::slotUpdateGlobalConfig);
    }
}


void NewForm::slotUpdateGlobalConfig()
{
    ui->robot3DVirtual->mGlobalConfig.isDrawGrid = getIsRealGridChecked();
    ui->robot3DVirtual->mGlobalConfig.isDrawWorldCoord = getIsRealWorldCoord();
    ui->robot3DVirtual->mGlobalConfig.isDrawJoint1Coord = getIsJointChecked(1);
    ui->robot3DVirtual->mGlobalConfig.isDrawJoint2Coord = getIsJointChecked(2);
    ui->robot3DVirtual->mGlobalConfig.isDrawJoint3Coord = getIsJointChecked(3);
    ui->robot3DVirtual->mGlobalConfig.isDrawJoint4Coord = getIsJointChecked(4);
    ui->robot3DVirtual->updateGL();
}

//选择绘制网格
bool NewForm::getIsRealGridChecked() const
{
    return ui->checkBoxGrid->isChecked();
}

//选择绘制世界坐标
bool NewForm::getIsRealWorldCoord() const
{
    return ui->checkBoxWorldCoord->isChecked();
}

//选择绘制各个部件单独的坐标系
bool NewForm::getIsJointChecked(int jointIndex) const
{
    switch (jointIndex) {
    case 1: return ui->checkBoxJoint1Coord->isChecked();
    case 2: return ui->checkBoxJoint2Coord->isChecked();
    case 3: return ui->checkBoxJoint3Coord->isChecked();
    case 4: return ui->checkBoxJoint4Coord->isChecked();
    default: return false;
    }
}

NewForm::~NewForm()
{
    delete ui;
}



//横移
void NewForm::on_sliderJoint1_valueChanged(int value)
{
    ui->robot3DVirtual->mRobotConfig.z[1] = value;
    ui->robot3DVirtual->updateGL();
}


//纵移
void NewForm::on_sliderJoint2_valueChanged(int value)
{
    ui->robot3DVirtual->mRobotConfig.z[2] = value;
    ui->robot3DVirtual->updateGL();
}


//托举
void NewForm::on_sliderJoint3_valueChanged(int value)
{
    ui->robot3DVirtual->mRobotConfig.y[2] = value;
    ui->robot3DVirtual->updateGL();
}


//手指
void NewForm::on_sliderJoint4_valueChanged(int value)
{
    ui->robot3DVirtual->mRobotConfig.z[4] = 440 - value;
    ui->robot3DVirtual->mRobotConfig.z[3] = value;
    ui->robot3DVirtual->updateGL();
}

