#include "levelmonitor.h"
#include "ui_levelmonitor.h"
#include "zmotioncontrol.h"

LevelMonitor::LevelMonitor(ZMotionControl* zm,QWidget *parent)
    : QMainWindow(parent), ui(new Ui::LevelMonitor),m_zm(zm)
{
    ui->setupUi(this);
}

LevelMonitor::~LevelMonitor()
{
    delete ui;
}

//同步传感器数据
void LevelMonitor::UpdateForceData(const QVariantMap &data)
{
    foreach(const QString &key, data.keys()) {
        float value = data[key].toFloat();
        if (key == "sensor0") {
            ui->sensor0->setText(QString::number(value, 'f', 4));
        } else if (key == "sensor1") {
            ui->sensor1->setText(QString::number(value, 'f', 4));
        } else if (key == "sensor2") {
            ui->sensor2->setText(QString::number(value, 'f', 4));
        } else if (key == "sensor3") {
            ui->sensor3->setText(QString::number(value, 'f', 4));
        }
    }
}

//同步角度数据
void LevelMonitor::UpdateAngleData(const QVariantMap &data)
{
    if (data.contains("AngleX")) {
        ui->AngleX->setText(QString::number(data["AngleX"].toFloat(), 'f', 4));
    }
    if (data.contains("AngleY")) {
        ui->AngleY->setText(QString::number(data["AngleY"].toFloat(), 'f', 4));
    }
}

//同步电流数据
void LevelMonitor::CurrentUpdate(float current0, float current1, float current2, float current3)
{
    if(m_zm->GetConnectStatus())
    {
        ui->Current0->setText(QString::number(current0, 'f', 2));
        ui->Current1->setText(QString::number(current1, 'f', 2));
        ui->Current2->setText(QString::number(current2, 'f', 2));
        ui->Current3->setText(QString::number(current3, 'f', 2));
    }
}

//电机状态
void LevelMonitor::onMotorStatusUpdated()
{
    for(int i = 0; i < 4; ++i)
    {
        // 轴0开始
        int axisNum = i ;

        QLabel* statusLbl = findChild<QLabel*>(QString("lblAxisStatus%1").arg(axisNum));
        if(statusLbl) statusLbl->setText(AxisStatusQstr(m_zm->GetAxisStatus(axisNum)));

        QLabel* posiLbl = findChild<QLabel*>(QString("lblPosi%1").arg(axisNum));
        if(posiLbl) posiLbl->setText(QString::number(m_zm->GetAxisDpos(axisNum)));

        QLabel* unitsLbl = findChild<QLabel*>(QString("lblAxisUnits%1").arg(axisNum));
        if(unitsLbl) unitsLbl->setText(QString::number(m_zm->GetAxisUnits(axisNum)));

        QLabel* speedLbl = findChild<QLabel*>(QString("lblAxisSpeed%1").arg(axisNum));
        if(speedLbl) speedLbl->setText(QString::number(m_zm->GetAxisSpeed(axisNum)));

        QLabel* accelLbl = findChild<QLabel*>(QString("lblAxisAccel%1").arg(axisNum));
        if(accelLbl) accelLbl->setText(QString::number(m_zm->GetAxisAccel(axisNum)));

        QLabel* decelLbl = findChild<QLabel*>(QString("lblAxisDecel%1").arg(axisNum));
        if(decelLbl) decelLbl->setText(QString::number(m_zm->GetAxisDecel(axisNum)));

        QLabel* fsLimitLbl = findChild<QLabel*>(QString("lblFsLimit%1").arg(axisNum));
        if(fsLimitLbl) fsLimitLbl->setText(QString::number(m_zm->GetAxisFSLimit(axisNum)));

        QLabel* bsLimitLbl = findChild<QLabel*>(QString("lblBsLimit%1").arg(axisNum));
        if(bsLimitLbl) bsLimitLbl->setText(QString::number(m_zm->GetAxisRSLimit(axisNum)));

        QLabel* typeLbl = findChild<QLabel*>(QString("lblAxisType%1").arg(axisNum));
        if(typeLbl) typeLbl->setText(QString::number(m_zm->GetAxisMType(axisNum)));
    }
}

//标志位
void LevelMonitor::onLevelSymbolChanged(LevelSymbol symbol)
{
    QString baseStyle = "border-radius: 7px; border: 2px solid black;background-color: white;";

    switch(symbol) {
    case LevelSymbol::FeetToGround0:ui->lbl_Symbol0->setStyleSheet(baseStyle.replace("white","green"));break;
    case LevelSymbol::FeetToGround1:ui->lbl_Symbol1->setStyleSheet(baseStyle.replace("white","green"));break;
    case LevelSymbol::FeetToGround2:ui->lbl_Symbol2->setStyleSheet(baseStyle.replace("white","green"));break;
    case LevelSymbol::FeetToGround3:ui->lbl_Symbol3->setStyleSheet(baseStyle.replace("white","green"));break;
    case LevelSymbol::Balance:ui->lbl_SymbolAll->setStyleSheet(baseStyle.replace("white","green"));break;
    case LevelSymbol::Restart:
        ui->lbl_Symbol0->setStyleSheet(baseStyle);
        ui->lbl_Symbol1->setStyleSheet(baseStyle);
        ui->lbl_Symbol2->setStyleSheet(baseStyle);
        ui->lbl_Symbol3->setStyleSheet(baseStyle);
        ui->lbl_SymbolAll->setStyleSheet(baseStyle);
        break;
    }
}
