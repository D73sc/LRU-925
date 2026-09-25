#include "liftmonitor.h"
#include "ui_liftmonitor.h"


LiftMonitor::LiftMonitor(ZMotionControl* zm,QWidget *parent)
    : QMainWindow(parent), ui(new Ui::LiftMonitor), m_zm(zm)
{
    ui->setupUi(this);
    IsCollsionTimer = new QTimer(this);

}

LiftMonitor::~LiftMonitor()
{
    delete ui;
}

void LiftMonitor::updateSixAxisSensorInfo(SixForeData* data)
{
    ui->lbl_FX->setText(QString::number(data->Fx, 'f', 3));
    ui->lbl_FY->setText(QString::number(data->Fy, 'f', 3));
    ui->lbl_FZ->setText(QString::number(data->Fz, 'f', 3));
    ui->lbl_MX->setText(QString::number(data->Mx, 'f', 3));
    ui->lbl_MY->setText(QString::number(data->My, 'f', 3));
    ui->lbl_MZ->setText(QString::number(data->Mz, 'f', 3));
}


//同步电机状态
void LiftMonitor::onMotorStatusUpdated()
{
    ui->lblAxisStatus4->setText(AxisStatusQstr(m_zm->GetAxisStatus(4)));
    ui->lblPosi4->setText(QString::number(m_zm->GetAxisDpos(4)));
    ui->lblAxisUnits4->setText(QString::number(m_zm->GetAxisUnits(4)));
    ui->lblAxisSpeed4->setText(QString::number(m_zm->GetAxisSpeed(4)));
    ui->lblAxisAccel4->setText(QString::number(m_zm->GetAxisAccel(4)));
    ui->lblAxisDecel4->setText(QString::number(m_zm->GetAxisDecel(4)));
    ui->lblFsLimit4->setText(QString::number(m_zm->GetAxisFSLimit(4)));
    ui->lblBsLimit4->setText(QString::number(m_zm->GetAxisRSLimit(4)));
    ui->lblAxisType4->setText(QString::number(m_zm->GetAxisMType(4)));

    ui->lblAxisStatus12->setText(AxisStatusQstr(m_zm->GetAxisStatus(12)));
    ui->lblPosi12->setText(QString::number(m_zm->GetAxisDpos(12)));
    ui->lblAxisUnits12->setText(QString::number(m_zm->GetAxisUnits(12)));
    ui->lblAxisSpeed12->setText(QString::number(m_zm->GetAxisSpeed(12)));
    ui->lblAxisAccel12->setText(QString::number(m_zm->GetAxisAccel(12)));
    ui->lblAxisDecel12->setText(QString::number(m_zm->GetAxisDecel(12)));
    ui->lblFsLimit12->setText(QString::number(m_zm->GetAxisFSLimit(12)));
    ui->lblBsLimit12->setText(QString::number(m_zm->GetAxisRSLimit(12)));
    ui->lblAxisType12->setText(QString::number(m_zm->GetAxisMType(12)));

}


//同步碰撞标志位
void LiftMonitor::UpdateCollsionUI()
{
    connect(IsCollsionTimer,&QTimer::timeout,this,[=](){

        if (IsCollsion)
        {
            ui->lbl_CrashInfo->setText("检测到碰撞，恢复至安全位置和姿态！");
            QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: red;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
        }
        else
        {
            ui->lbl_CrashInfo->setText("当前未检测到碰撞");
            QString baseStyle = "border-radius: 25px; border: 2px solid black; background-color: green;";
            ui->lbl_isCrash->setStyleSheet(baseStyle);
        }

    });

    IsCollsionTimer->start(100);
}

void LiftMonitor::updateSixDofPlatformUI(SixDofPlatformData& data)
{
    // 更新六自由度平台信息
    ui->lbl_PlatformInfoX->setText(QString::number(data.x));
    ui->lbl_PlatformInfoY->setText(QString::number(data.y));
    ui->lbl_PlatformInfoZ->setText(QString::number(data.z));
    ui->lbl_PlatformInfoRx->setText(QString::number(data.rx));
    ui->lbl_PlatformInfoRy->setText(QString::number(data.ry));
    ui->lbl_PlatformInfoRz->setText(QString::number(data.rz));
    ui->lbl_PlatformStatus->setText(QString::number(data.motion_mode));
    ui->lbl_PlatformEnable->setText(data.servo_enabled ? "True" : "False");
    ui->lbl_PlatformIsTarget->setText(data.target_reached ? "True" : "False");
    ui->lbl_PlatformWarning->setText(data.alarmText);
}

void LiftMonitor::updateDetectedDisplayUI(const DetectedDisplayData& data)
{
    // 更新视觉信息
    ui->lbl_GapAngle->setText(QString::number(data.angle));
    ui->lbl_GapWidth->setText(QString::number(data.rho1));
    ui->lbl_GapWidth1->setText(QString::number(data.rho2));
}
