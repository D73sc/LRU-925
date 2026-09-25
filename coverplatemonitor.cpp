#include "coverplatemonitor.h"
#include "ui_coverplatemonitor.h"

CoverPlateMonitor::CoverPlateMonitor(ZMotionControl* zm,QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CoverPlateMonitor),m_zm(zm)
{
    ui->setupUi(this);
}

CoverPlateMonitor::~CoverPlateMonitor()
{
    delete ui;
}

//电机状态更新
void CoverPlateMonitor::onMotorStatusUpdated()
{
    for(int i = 0; i < 7; ++i)
    {
        // 轴5开始
        int axisNum = i + 5;

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
void CoverPlateMonitor::onCoverPlateSymbolChanged(CoverPlateSymbol symbol) {
    QString baseStyle = "border-radius: 7px; border: 2px solid black;background-color: white;";
    resetCoverPlateSymbol();
    switch (symbol) {
    case CoverPlateSymbol::TakeDoorComplete:ui->lbl_TakeDoor->setStyleSheet(baseStyle.replace("white","green"));break;
    case CoverPlateSymbol::TakePlateComplete:ui->lbl_TakePlate->setStyleSheet(baseStyle.replace("white","green"));break;
    case CoverPlateSymbol::CoverPlateComplete:ui->lbl_FixPlate->setStyleSheet(baseStyle.replace("white","green"));break;
    case CoverPlateSymbol::CoverDoorComplete:ui->lbl_FixDoor->setStyleSheet(baseStyle.replace("white","green"));break;
    }
}


void CoverPlateMonitor::resetCoverPlateSymbol() {
    QString baseStyle = "border-radius: 7px; border: 2px solid black; background-color: white;";
    ui->lbl_TakeDoor->setStyleSheet(baseStyle);
    ui->lbl_TakePlate->setStyleSheet(baseStyle);
    ui->lbl_FixPlate->setStyleSheet(baseStyle);
    ui->lbl_FixDoor->setStyleSheet(baseStyle);
}

