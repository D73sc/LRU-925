#ifndef COVERPLATEFSM_H
#define COVERPLATEFSM_H

#include <boost/msm/front/state_machine_def.hpp>
#include<cmath>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>
#include <QtGlobal>
#include <cmath>
#include <vector>
#include <QObject>
#include <QTimer>
#include <QDebug>
#include <functional>
#include <zmotioncontrol.h>
#include <vision_source/dmdetected.h>
#include<QSettings>


namespace GlobalMotorPos {

static float coverPlatePos[4] = {0.0f};

static float borrowPos[2] = {0.0f};

//手指位置判断
static float FingerPos[2] = {0.0f};

static bool leftSymbol = false;

static bool rightSymbol = false;

static bool center_rSymbol = false;

static bool center_lSymbol = false;

static float DMVal[3] = {0.0f};

// 存储4个电机位置
inline void setCoverPlatePosBatch(float pos5, float pos6, float pos7, float pos8) {
    coverPlatePos[0] = pos5;
    coverPlatePos[1] = pos6;
    coverPlatePos[2] = pos7;
    coverPlatePos[3] = pos8;
}

inline void setBorrowPosBatch(float pos5, float pos6) {
    borrowPos[0] = pos5;
    borrowPos[1] = pos6;
}

inline void setFingerPosBatch(float pos9, float pos10) {
    FingerPos[0] = pos9;
    FingerPos[1] = pos10;
}

// 获取电机位置
inline float getCoverPlatePos(int idx) {
    return coverPlatePos[idx];
}

inline float getBorrowPos(int idx) {
    return borrowPos[idx];
}

inline float getFingerPos(int idx) {
    return FingerPos[idx];
}

//获取二维码坐标
inline void setDmValues(float val0, float val1, float val2) {
    DMVal[0] = val0;
    DMVal[1] = val1;
    DMVal[2] = val2;
}

inline float getDmValues(int idx) {
    return DMVal[idx];
}

inline void setCoverPlateSymbol(bool symbol) {
    leftSymbol = symbol;
    rightSymbol = !symbol;
}

inline void setCenterSymbol(bool symbol) {
    center_rSymbol = symbol;
    center_lSymbol = symbol;
}

inline void resetCoverPlateSymbol() {
    leftSymbol = false;
    rightSymbol = false;
    center_rSymbol = false;
    center_lSymbol = false;
}
}


//取盖板电机逻辑——实际运动映射关系
//托举电机: Forward-下降
//横移电机: 均设置Forward-靠近电机端
//手指电机: 均设置Forward-两手指远离
//纵移电机: 均设置Forward-靠近电机端
// 前向声明
class ZMotionControl;
class DMDetected;
// 事件定义*************************************************************************************************************
struct AngleAdjustDone{};
struct XAdjustDone{};
struct YAdjustDone{};
struct XAdjustAdvanceDone{};
struct YAdjustAdvanceDone{};
struct LiftingDone {};
struct LoweringDone {};
struct LoweringDone_55 {};
struct FingerReleasingDone {};
struct FingerClampingDone {};
struct FingerNear1Done{};
struct FingerNear2Done{};
struct FingerClampingDurDone {};//手指持续移动
struct FingerClampingOppDone {};//手指相对移动
struct HangingDone {};//悬挂移动
struct AntiHangingDone {};//摘取移动
struct ForwardTransverseMovementDone {};
struct ReverseTransverseMovementDone {};
struct ForwardLongitudinalMovementDone {};
struct ReverseLongitudinalMovementDone {};
struct ReverseTransverseMovementTo100{};
struct LongitudinalMovementDone{};
struct MyRecoverDone {};
struct MyPlateRecoverDone1 {};
struct MyPlateRecoverDone2 {};
struct ReverseLongitudinalForBorrowDone{};
struct ForwardLongitudinalForBorrowDone{};
struct ForwardLongitudinalForCenterBorrowDone{};
struct LongitudinalMoveto50Done{};

// 半自动流程确认守卫（新加）
struct WaitForOperatorConfirm
{
    template <class FSM, class Event, class SourceState, class TargetState>
    bool operator()(Event const&,
                    FSM& fsm,
                    SourceState&,
                    TargetState&) const
    {
        // 没有启用半自动时，保持原来的自动转换
        if (!fsm.m_stepConfirmEnabled) {
            return true;
        }

        // 操作员已经点击确认，允许进入下一状态
        if (fsm.m_stepConfirmed) {
            fsm.m_stepConfirmed = false;
            fsm.m_waitingStepConfirmation = false;
            return true;
        }

        // 当前动作第一次到位时通知界面
        if (!fsm.m_waitingStepConfirmation) {
            fsm.m_waitingStepConfirmation = true;

            emit fsm.sigStepConfirmationRequired(
                SourceState::name(),
                TargetState::name());
        }

        // 没确认，不允许状态转换
        return false;
    }
};

// 守卫条件*********************************************************************************************************
struct GuardLeftCoverPlate{
    // 左侧灯箱左移借位
    template <class FSM, class Event, class SourceState, class TargetState>
    bool operator()(Event const&, FSM& fsm, SourceState&, TargetState&) const {
        return GlobalMotorPos::leftSymbol;
    }
};

struct GuardRightCoverPlate{
    // 右侧盖板右移借位
    template <class FSM, class Event, class SourceState, class TargetState>
    bool operator()(Event const&, FSM& fsm, SourceState&, TargetState&) const {
        return GlobalMotorPos::rightSymbol;
    }
};

struct GuardCenter_lCoverPlate{
    // 中间盖板左移补偿借位
    template <class FSM, class Event, class SourceState, class TargetState>
    bool operator()(Event const&, FSM& fsm, SourceState&, TargetState&) const {
        return GlobalMotorPos::center_lSymbol;
    }
};

// 公共状态定义*******************************************************************************************************
//下降
struct Lowering : boost::msm::front::state<>
{
    static QString name() { return "下降至39"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 39); // 下降至39，防止封门压线
        emit fsm.sigStateChanged(name());
    }
};

//抬升
struct Lifting : boost::msm::front::state<>
{
    static QString name() { return "抬升至14"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 14); // 抬升至14，封门没过梢钉
        emit fsm.sigStateChanged(name());
    }
};

//正向横移
struct ForwardTransverseMovement : boost::msm::front::state<>
{
    static QString name() { return "正向横移至1120"; }//1120

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(7,1120);//1120
            fsm.moveMotorAbs(8,1120);
        }
        emit fsm.sigStateChanged(name());
    }
};

//反向横移
struct ReverseTransverseMovement : boost::msm::front::state<>
{
    static QString name() { return "反向横移至0"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(7,0);
            fsm.moveMotorAbs(8,0);
        }
        emit fsm.sigStateChanged(name());
    }
};

// //正向纵移，用于取装左侧盖板
// struct ForwardLongitudinalMovement : boost::msm::front::state<>
// {
//     static QString name() { return "正向纵移至65"; }

//     template <class Event,class FSM>
//     void on_entry(Event const&, FSM& fsm)
//     {
//         qDebug() << "进入状态:" << name();
//         if (fsm.m_enableEntryActions) {
//             fsm.moveMotorAbs(5,65);
//             fsm.moveMotorAbs(6,65);
//         }
//         emit fsm.sigStateChanged(name());
//     }
// };

//反向纵移，用于确保左中右侧盖板二维码视野
struct ReverseLongitudinalMovement : boost::msm::front::state<>
{
    static QString name() { return "持续纵移"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveSingleV(5);
            fsm.moveSingleV(6);
            QVector<float> currentVal = fsm.getDmValues();
            GlobalMotorPos::setDmValues(currentVal[0],currentVal[1],currentVal[2]);
        }
        emit fsm.sigStateChanged(name());
    }
};

//手指松开
struct FingerReleasing : boost::msm::front::state<>
{
    static QString name() { return "手指松开至10"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(9,10);
            fsm.moveMotorAbs(10,10);
        }
        emit fsm.sigStateChanged(name());
    }
};

//手指夹紧
struct FingerClamping : boost::msm::front::state<>
{
    static QString name() { return "手指夹紧至11.4"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(9,11.4);
            fsm.moveMotorAbs(10,11.4);
        }
        emit fsm.sigStateChanged(name());
    }
};

//手指持续夹紧
struct FingerClampingDur : boost::msm::front::state<>
{
    static QString name() { return "手指持续夹紧"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveSingleV(9);
            fsm.moveSingleV(10);
        }
        emit fsm.sigStateChanged(name());
    }
};

//手指相对夹紧
struct FingerClampingOpp : boost::msm::front::state<>
{
    static QString name() { return "手指相对夹紧"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            //记录手指位置
            float pos9 = fsm.getMotorPosition(9);
            float pos10 = fsm.getMotorPosition(10);
            GlobalMotorPos::setFingerPosBatch(pos9, pos10);
            fsm.moveMotorOpp(9,-12);//相对夹取距离
            fsm.moveMotorOpp(10,-12);
        }
        emit fsm.sigStateChanged(name());
    }
};

//取下封门移动
struct MoveForAntiHanging : boost::msm::front::state<>
{
    static QString name() { return "取封门横移至1120"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        fsm.moveMotorAbs(7,1120);//1120
        fsm.moveMotorAbs(8,1120);
        emit fsm.sigStateChanged(name());
    }
};

//封门悬挂移动
struct MoveForHanging : boost::msm::front::state<>
{
    static QString name() { return "挂封门横移至1145"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        fsm.moveMotorAbs(7,1145);//1145
        fsm.moveMotorAbs(8,1145);
        emit fsm.sigStateChanged(name());
    }
};

//角度调整
struct AngleAdjust : boost::msm::front::state<>//角度调整
{
    static QString name() { return "角度调整"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        // 角度调节前先停止持续运动
        fsm.moveSingleStop(5);
        fsm.moveSingleStop(6);

        qDebug() << "进入状态:" << name();
        QTimer::singleShot(500,&fsm,[&fsm](){
            if (fsm.m_enableEntryActions) {
                QVector<float> currentVals1 = fsm.getDmValues();
                float relativeAngle = (currentVals1[2] / 10 - 90) * 3.1415926 / 180;//初始为90度
                float distance = 980.0f * tan(relativeAngle);//两个纵移丝杆距离980
                fsm.moveMotorOpp(5, -distance);//固定电机6不动，只调节电机5；相对角度为正代表纵移机构有顺时针偏角，电机5向远离电机方向移动(backward)，距离为负
            }
            emit fsm.sigStateChanged(name());
        });
    }
};

//X调整
struct XAdjust : boost::msm::front::state<>//X坐标调整
{
    static QString name() { return "X坐标调整"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QTimer::singleShot(1000,&fsm,[&fsm](){
            if (fsm.m_enableEntryActions) {
                //x坐标调节
                QVector<float> currentVals2 = fsm.getDmValues();
                float relativeX = currentVals2[1] / 10;//x初始坐标为0
                fsm.moveMotorOpp(5, relativeX);//x坐标变大，代表5、6电机要向靠近电机端运动(Forward)
                fsm.moveMotorOpp(6, relativeX);
            }
            emit fsm.sigStateChanged(name());
        });
    }
};

//Y调整
struct YAdjust : boost::msm::front::state<>//Y坐标调整
{
    static QString name() { return "Y坐标调整";}

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QTimer::singleShot(1000,&fsm,[&fsm](){
            if (fsm.m_enableEntryActions) {
                //y坐标调节
                QVector<float> currentVals3 = fsm.getDmValues();
                float relativeY = currentVals3[0] / 10 - 4;//y初始坐标为0
                fsm.moveMotorOpp(7, relativeY);//y坐标变大，代表7、8电机要向远离电机端运动(Backward),距离为负
                fsm.moveMotorOpp(8, relativeY);
            }
            emit fsm.sigStateChanged(name());
        });
    }
};

//X预先调整
struct XAdjustAdvance : boost::msm::front::state<>//X坐标调整
{
    static QString name() { return "X坐标预调整"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QTimer::singleShot(1000,&fsm,[&fsm](){
            if (fsm.m_enableEntryActions) {
                //x坐标调节
                QVector<float> currentVals2 = fsm.getDmValues();
                float relativeX = currentVals2[1] / 10;//x初始坐标为0
                fsm.moveMotorOpp(5, relativeX);//x坐标变大，代表5、6电机要向靠近电机端运动(Forward)
                fsm.moveMotorOpp(6, relativeX);
            }
            emit fsm.sigStateChanged(name());
        });
    }
};

//Y预先调整
struct YAdjustAdvance : boost::msm::front::state<>//Y坐标调整
{
    static QString name() { return "Y坐标预调整";}

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QTimer::singleShot(1000,&fsm,[&fsm](){
            if (fsm.m_enableEntryActions) {
                //y坐标调节
                QVector<float> currentVals3 = fsm.getDmValues();
                float relativeY = currentVals3[0] / 10 - 4;//y初始坐标为0
                fsm.moveMotorOpp(7, relativeY);//y坐标变大，代表7、8电机要向远离电机端运动(Backward),距离为负
                fsm.moveMotorOpp(8, relativeY);
            }
            emit fsm.sigStateChanged(name());
        });
    }
};

//机构复原
struct MyRecover : boost::msm::front::state<>
{
    static QString name() { return "横、纵移机构初始"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        fsm.moveMotorAbs(5,81);
        fsm.moveMotorAbs(6,47);
        fsm.moveMotorAbs(7,0);
        fsm.moveMotorAbs(8,0);
        emit fsm.sigStateChanged(name());
    }
};

//完成状态
struct TakeFloorCompleted : boost::msm::front::state<>
{
    static QString name() { return "TakeFloorCompleted"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        emit fsm.sigStateChanged(name());
    }
};

struct TakePlateCompleted : boost::msm::front::state<>
{
    static QString name() { return "TakePlateCompleted"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        emit fsm.sigStateChanged(name());
    }
};

struct CoverPlateCompleted : boost::msm::front::state<>
{
    static QString name() { return "CoverPlateCompleted"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        emit fsm.sigStateChanged(name());
    }
};

struct CoverFloorCompleted : boost::msm::front::state<>
{
    static QString name() { return "CoverFloorCompleted"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        emit fsm.sigStateChanged(name());
    }
};


//专属状态
//取盖板状态机专属状态
namespace MyPlate {
struct Lowering1 : boost::msm::front::state<>
{
    static QString name() { return "取盖板下降至55"; }//15
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 55); // 下降至15，确保手指完全从封门伸出
        emit fsm.sigStateChanged(name());
    }
};

struct Lowering2 : boost::msm::front::state<>
{
    static QString name() { return "取盖板下降至23"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 23); // 下降至23，确保盖板低于封门位置
        emit fsm.sigStateChanged(name());
    }
};

struct MoveTo_55 : boost::msm::front::state<>
{
    static QString name() { return "取盖高度至58"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 58);
        emit fsm.sigStateChanged(name());
    }
};

struct Lifting1 : boost::msm::front::state<>
{
    static QString name() { return "取盖板上升至0"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();

        float pos5 = fsm.getMotorPosition(5);
        float pos6 = fsm.getMotorPosition(6);
        float pos7 = fsm.getMotorPosition(7);
        float pos8 = fsm.getMotorPosition(8);
        QSettings * iniReadPOS = new QSettings("param.ini", QSettings::IniFormat);
        iniReadPOS->setValue("/Motorpos/Pos5",QString::number(pos5,'f',3));
        iniReadPOS->setValue("/Motorpos/Pos6",QString::number(pos6,'f',3));
        iniReadPOS->setValue("/Motorpos/Pos7",QString::number(pos7,'f',3));
        iniReadPOS->setValue("/Motorpos/Pos8",QString::number(pos8,'f',3));
        GlobalMotorPos::setCoverPlatePosBatch(pos5, pos6, pos7, pos8);

        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 0); // 抬升至-95，确保手指伸入盖板孔位
        emit fsm.sigStateChanged(name());
        emit fsm.sigStateChanged(QString("记录横移位置：%1").arg(pos7));
        emit fsm.sigStateChanged(QString("记录纵移位置1：%1").arg(pos5));
        emit fsm.sigStateChanged(QString("记录纵移位置2：%1").arg(pos6));
    }
};

//手指靠拢
struct FingerNear1 : boost::msm::front::state<>
{
    static QString name() { return "手指靠拢至-22"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(9,-22);
            fsm.moveMotorAbs(10,-22);
        }
        emit fsm.sigStateChanged(name());
    }
};

//手指夹紧
struct FingerNear2 : boost::msm::front::state<>
{
    static QString name() { return "手指靠拢至-50"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(9,-50);
            fsm.moveMotorAbs(10,-50);
        }
        emit fsm.sigStateChanged(name());
    }
};

//反向横移至100
struct ReverseTransverseMovement : boost::msm::front::state<>
{
    static QString name() { return "反向横移至100"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(7,100);
            fsm.moveMotorAbs(8,100);
        }
        emit fsm.sigStateChanged(name());
    }
};


//纵移至50
struct LongitudinalMoveto50 : boost::msm::front::state<>
{
    static QString name() { return "纵移至50"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(5,50);
            fsm.moveMotorAbs(6,50);
        }
        emit fsm.sigStateChanged(name());
    }
};


//纵移回零
struct LongitudinalMovement : boost::msm::front::state<>
{
    static QString name() { return "纵移至0"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorAbs(5,0);
            fsm.moveMotorAbs(6,0);
        }
        emit fsm.sigStateChanged(name());
    }
};

//盖盖板姿态恢复
struct MyPlateRecover1: boost::msm::front::state<>
{
    static QString name() { return "横移姿态恢复"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QSettings * iniReadPOS = new QSettings("param.ini", QSettings::IniFormat);
        if (fsm.m_enableEntryActions){
            float pos7 =iniReadPOS->value("/Motorpos/Pos7").toString().toFloat();
            float pos8 =iniReadPOS->value("/Motorpos/Pos8").toString().toFloat();
            fsm.moveMotorAbs(7, pos7);
            fsm.moveMotorAbs(8, pos8);
            emit fsm.sigStateChanged(QString("恢复横移坐标：%1").arg(pos7));
        }
        emit fsm.sigStateChanged(name());
    }
};

struct MyPlateRecover2: boost::msm::front::state<>
{
    static QString name() { return "纵移姿态恢复"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        QSettings * iniReadPOS = new QSettings("param.ini", QSettings::IniFormat);
        if (fsm.m_enableEntryActions){
            float pos5 =iniReadPOS->value("/Motorpos/Pos5").toString().toFloat();
            float pos6 =iniReadPOS->value("/Motorpos/Pos6").toString().toFloat();
            fsm.moveMotorAbs(5, pos5);
            fsm.moveMotorAbs(6, pos6);
            emit fsm.sigStateChanged(QString("恢复纵移坐标1：%1").arg(pos5));
            emit fsm.sigStateChanged(QString("恢复纵移坐标2：%1").arg(pos6));
        }
        emit fsm.sigStateChanged(name());
    }
};

//纵移补偿，用于取装左侧盖板
struct ForwardLongitudinalForBorrow : boost::msm::front::state<>
{
    static QString name() { return "正向相对纵移324"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        float pos5 = fsm.getMotorPosition(5);
        float pos6 = fsm.getMotorPosition(6);
        GlobalMotorPos::setBorrowPosBatch(pos5, pos6);

        emit fsm.sigStateChanged(QString("记录纵移位置1：%1").arg(pos5));
        emit fsm.sigStateChanged(QString("记录纵移位置2：%1").arg(pos6));

        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorOpp(5,324);
            fsm.moveMotorOpp(6,324);
        }
        emit fsm.sigStateChanged(name());
    }
};

//纵移补偿，用于取装中间盖板
struct ForwardLongitudinalForCenterBorrow : boost::msm::front::state<>
{
    static QString name() { return "正向相对纵移320"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        float pos5 = fsm.getMotorPosition(5);
        float pos6 = fsm.getMotorPosition(6);
        GlobalMotorPos::setBorrowPosBatch(pos5, pos6);

        emit fsm.sigStateChanged(QString("记录纵移位置1：%1").arg(pos5));
        emit fsm.sigStateChanged(QString("记录纵移位置2：%1").arg(pos6));

        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorOpp(5,320);
            fsm.moveMotorOpp(6,320);
        }
        emit fsm.sigStateChanged(name());
    }
};


//纵移补偿，用于取装右侧盖板
struct ReverseLongitudinalForBorrow : boost::msm::front::state<>
{
    static QString name() { return "反向相对纵移-324"; }

    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        float pos5 = fsm.getMotorPosition(5);
        float pos6 = fsm.getMotorPosition(6);
        GlobalMotorPos::setBorrowPosBatch(pos5, pos6);

        emit fsm.sigStateChanged(QString("记录纵移位置1：%1").arg(pos5));
        emit fsm.sigStateChanged(QString("记录纵移位置2：%1").arg(pos6));

        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions) {
            fsm.moveMotorOpp(5,-324);
            fsm.moveMotorOpp(6,-324);
        }
        emit fsm.sigStateChanged(name());
    }
};
}


// 盖封门状态机专属状态
namespace MyCoverFloor {
struct Lowering1 : boost::msm::front::state<>
{
    static QString name() { return "盖封门下降至15"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 15); // 下降至15，确保手指低于封门
        emit fsm.sigStateChanged(name());
    }
};

struct Lifting1 : boost::msm::front::state<>
{
    static QString name() { return "盖封门上升至18"; }
    template <class Event,class FSM>
    void on_entry(Event const&, FSM& fsm)
    {
        qDebug() << "进入状态:" << name();
        if (fsm.m_enableEntryActions)
            fsm.moveMotorAbs(11, 18); // 抬升至18，确保封门密封良好
        emit fsm.sigStateChanged(name());
    }
};
}

// 非模板QObject基类
class BaseFSM_QObject : public QObject {
    Q_OBJECT
signals:
    void sigStateChanged(const QString& stateName);

    void sigStepConfirmationRequired(const QString& currentState,const QString& nextState);//新加

public:
    explicit BaseFSM_QObject(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : QObject(parent), m_zm(zm), m_dm(dm) {}

    ZMotionControl* m_zm;
    DMDetected* m_dm;
};


// 模板类
template <class Derived>
class BaseFSM : public BaseFSM_QObject, public boost::msm::front::state_machine_def<Derived> {
public:
    BaseFSM(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : BaseFSM_QObject(parent, zm, dm) {}
    //新加907-929
    bool m_stepConfirmEnabled = false;
    bool m_waitingStepConfirmation = false;
    bool m_stepConfirmed = false;

    void setStepConfirmEnabled(bool enabled)
    {
        m_stepConfirmEnabled = enabled;
        m_waitingStepConfirmation = false;
        m_stepConfirmed = false;
    }

    void confirmNextStep()
    {
        if (m_stepConfirmEnabled &&
            m_waitingStepConfirmation) {
            m_stepConfirmed = true;
        }
    }

    bool isWaitingStepConfirmation() const
    {
        return m_waitingStepConfirmation;
    }

    // 用于手动模式下，只进行状态转换，不进入运控逻辑
    bool m_enableEntryActions = true;

    // 存储电机位置坐标，用于盖盖板回复姿态
    float motorPos[4] = {0.0f};

    // 外部接口
    std::function<void(int motor, float position)> moveMotorAbs;       // 电机绝对位置移动
    std::function<void(int motor, float position)> moveMotorOpp;       // 电机相对位置移动
    std::function<void(int motor)> moveSingleV;                        // 持续运动
    std::function<void(int motor)> moveSingleStop;                     // 停止
    std::function<float(int motor)> getMotorPosition;                  // 获取电机位置
    std::function<QVector<float>()> getDmValues;                      // 获取DM解析后的数值

    // 重写no_transition，状态机接收到没有转换规则的事件不进行任何动作，直接忽略
    template <class Event, class StateMachine>
    void no_transition(Event const& /*e*/, StateMachine& /*sm*/, int /*state*/) {
    }
};

// 基础版本均基于TM1、TM2
// 取封门状态机
class TakeFloorFSMFront : public BaseFSM<TakeFloorFSMFront>
{
    Q_OBJECT
public:
    TakeFloorFSMFront(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : BaseFSM(parent, zm, dm) {}

    // 初始状态
    typedef Lowering initial_state;

    // 状态转换表（新修加了守卫条件）
    struct transition_table : boost::mpl::vector5<
                                  // 下降至39[防止封门压线]-> 正向横移至1120
                                  boost::msm::front::Row<Lowering, LoweringDone, ForwardTransverseMovement,boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 正向横移至1120 -> 抬升至14
                                  boost::msm::front::Row<ForwardTransverseMovement, ForwardTransverseMovementDone, Lifting,boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 抬升至14 -> 悬挂移动至1145
                                  boost::msm::front::Row<Lifting, LiftingDone, MoveForHanging,boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 悬挂移动至1145 -> 手指松开至10
                                  boost::msm::front::Row<MoveForHanging, HangingDone, FingerReleasing,boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 手指松开至10 -> 取封门完成
                                  boost::msm::front::Row<FingerReleasing, FingerReleasingDone, TakeFloorCompleted,boost::msm::front::none, WaitForOperatorConfirm>> {};
};


// 取盖板状态机
class TakePlateFSMFront : public BaseFSM<TakePlateFSMFront>
{
    Q_OBJECT
public:
    TakePlateFSMFront(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : BaseFSM(parent, zm, dm) {}

    // 初始状态
    typedef MyPlate::Lowering1 initial_state;

    // 状态转换表
    struct transition_table : boost::mpl::vector15<
                                  // 下降至55[确保手指从封门中完全取出] -> 反向横移至100,保证横移调整的裕度
                                  boost::msm::front::Row<MyPlate::Lowering1, LoweringDone, MyPlate::ReverseTransverseMovement,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 反向横移至100 -> 纵移至50
                                  boost::msm::front::Row<MyPlate::ReverseTransverseMovement, ReverseTransverseMovementTo100, MyPlate::LongitudinalMoveto50,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 纵移至50 -> 手指靠拢至-22对照盖板孔位
                                  boost::msm::front::Row<MyPlate::LongitudinalMoveto50, LongitudinalMoveto50Done, MyPlate::FingerNear1,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 手指靠拢至-22 -> 调整纵移，保证二维码视野,左中右盖板均适用
                                  boost::msm::front::Row<MyPlate::FingerNear1, FingerNear1Done, ReverseLongitudinalMovement,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 调整纵移 -> X预调整状态
                                  boost::msm::front::Row<ReverseLongitudinalMovement, ReverseLongitudinalMovementDone, XAdjustAdvance,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // X预调整状态 -> Y预调整状态
                                  boost::msm::front::Row<XAdjustAdvance, XAdjustAdvanceDone, YAdjustAdvance,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // Y预调整状态 -> 角度调整状态
                                  boost::msm::front::Row<YAdjustAdvance, YAdjustAdvanceDone, AngleAdjust,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 角度调整状态 -> X调整状态
                                  boost::msm::front::Row<AngleAdjust, AngleAdjustDone, XAdjust,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // X调整状态 -> Y调整状态
                                  boost::msm::front::Row<XAdjust, XAdjustDone, YAdjust,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // Y调整状态 -> 抬升至0
                                  boost::msm::front::Row<YAdjust, YAdjustDone, MyPlate::Lifting1,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 抬升至0 -> 手指持续收缩
                                  boost::msm::front::Row<MyPlate::Lifting1, LiftingDone, FingerClampingDur,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 手指持续收缩 -> 下降至58
                                  boost::msm::front::Row<FingerClampingDur, FingerClampingDurDone, MyPlate::MoveTo_55,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 下降至58 -> 纵移至0
                                  boost::msm::front::Row<MyPlate::MoveTo_55, LoweringDone_55, MyPlate::LongitudinalMovement,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 纵移至0 -> 正向横移至1132
                                  boost::msm::front::Row<MyPlate::LongitudinalMovement, LongitudinalMovementDone, ForwardTransverseMovement,
                                                         boost::msm::front::none, boost::msm::front::none>,
                                  // 正向横移至1132 -> 取盖板完成
                                  boost::msm::front::Row<ForwardTransverseMovement, ForwardTransverseMovementDone, TakePlateCompleted,
                                                         boost::msm::front::none, boost::msm::front::none>> {};
};


// 盖盖板状态机
class CoverPlateFSMFront : public BaseFSM<CoverPlateFSMFront>
{
    Q_OBJECT
public:
    CoverPlateFSMFront(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : BaseFSM(parent, zm, dm) {}

    // 初始状态
    typedef MyPlate::MyPlateRecover1 initial_state;

    // 状态转换表
    struct transition_table : boost::mpl::vector5<
                                  // 横移姿态恢复 -> 纵移姿态恢复
                                  boost::msm::front::Row<MyPlate::MyPlateRecover1, MyPlateRecoverDone1, MyPlate::MyPlateRecover2>,
                                  // 纵移姿态恢复 -> 抬升至0
                                  boost::msm::front::Row<MyPlate::MyPlateRecover2, MyPlateRecoverDone2, MyPlate::Lifting1>,
                                  // 抬升至0 -> 手指松开至-25
                                  boost::msm::front::Row<MyPlate::Lifting1, LiftingDone, MyPlate::FingerNear1>,
                                  // 手指松开至-25 -> 下降至55,确保手指不会碰撞封门
                                  boost::msm::front::Row<MyPlate::FingerNear1, FingerNear1Done, MyPlate::Lowering1,
                                                         boost::msm::front::none,boost::msm::front::none>,
                                  // 下降至55 -> 盖盖板完成
                                  boost::msm::front::Row<MyPlate::Lowering1, LoweringDone, CoverPlateCompleted>> {};
};

// 盖封门状态机，由于此时已经调整过位姿来取盖板，因此盖封门前要先恢复至纵移机构的零位
class CoverFloorFSMFront : public BaseFSM<CoverFloorFSMFront>
{
    Q_OBJECT
public:
    CoverFloorFSMFront(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : BaseFSM(parent, zm, dm) {}

    // 初始状态
    typedef MyRecover initial_state;

    // 状态转换表（新加守卫条件）
    struct transition_table : boost::mpl::vector10<
                                  // 恢复机构初始 -> 手指松开至10
                                  boost::msm::front::Row<MyRecover, MyRecoverDone, FingerReleasing,
                                                         boost::msm::front::none,WaitForOperatorConfirm>,
                                  // 手指松开至10 -> 正向横移至1120
                                  boost::msm::front::Row<FingerReleasing, FingerReleasingDone, ForwardTransverseMovement,
                                                         boost::msm::front::none,WaitForOperatorConfirm>,
                                  // 正向横移至1120 -> 悬挂移动至1145
                                  boost::msm::front::Row<ForwardTransverseMovement, ForwardTransverseMovementDone, MoveForHanging,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 悬挂移动至1145 -> 抬升至14
                                  boost::msm::front::Row<MoveForHanging, HangingDone, Lifting,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 抬升至14 -> 手指夹紧至11.4
                                  boost::msm::front::Row<Lifting, LiftingDone, FingerClamping,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 手指夹紧至11.4 -> 摘取移动至1120
                                  boost::msm::front::Row<FingerClamping, FingerClampingDone, MoveForAntiHanging,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 摘取移动至1120 -> 下降至39[防止封门压线]
                                  boost::msm::front::Row<MoveForAntiHanging, AntiHangingDone, Lowering,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 下降至39 -> 反向横移至0
                                  boost::msm::front::Row<Lowering, LoweringDone, ReverseTransverseMovement,
                                                         boost::msm::front::none ,WaitForOperatorConfirm>,
                                  // 反向横移至0 -> 抬升至18
                                  boost::msm::front::Row<ReverseTransverseMovement, ReverseTransverseMovementDone, MyCoverFloor::Lifting1,
                                                         boost::msm::front::none, WaitForOperatorConfirm>,
                                  // 抬升至18 -> 盖封门完成
                                  boost::msm::front::Row<MyCoverFloor::Lifting1, LiftingDone, CoverFloorCompleted,
                                                         boost::msm::front::none, WaitForOperatorConfirm>> {};
};

// 状态机后端实现
template <class FrontFSM>
class FSMBackend : public boost::msm::back::state_machine<FrontFSM>
{
public:
    FSMBackend(QObject* parent = nullptr, ZMotionControl* zm = nullptr, DMDetected* dm = nullptr)
        : boost::msm::back::state_machine<FrontFSM>(), m_zm(zm), m_dm(dm) {

        FrontFSM& frontFSM = *this;

        // 连接状态变化信号到更新函数
        QObject::connect(&frontFSM, &BaseFSM_QObject::sigStateChanged,
                         [this](const QString& stateName) {
                             m_currentStateName = stateName;
                         });


        // 运动控制接口
        frontFSM.moveMotorAbs = [this](int motor, float position) {
            if (m_zm)
                m_zm->MoveSingleAbs(motor, position);
        };

        frontFSM.moveMotorOpp = [this](int motor, float position) {
            if (m_zm)
                m_zm->MoveSingleOpp(motor, position);
        };

        frontFSM.moveSingleV = [this](int motor) {
            if (m_zm)
                m_zm->SingleVMove(motor, Backward);
        };

        frontFSM.moveSingleStop = [this](int motor) {
            if (m_zm)
                m_zm->SingleVMove(motor, Cancel);
        };

        frontFSM.getMotorPosition = [this](int motor) {
            if (m_zm)
                return m_zm->GetAxisDpos(motor);
            return 0.0f; // 添加默认返回值
        };

        // DM解析值获取接口
        frontFSM.getDmValues = [this]() {
            if (m_dm)
                return m_dm->getValues();
            return QVector<float>(); // 默认返回空向量
        };

        m_zm = zm;
    }

    // 提供访问前端的接口
    FrontFSM* operator->() { return &this;}
    FrontFSM& getFrontFSM() { return *this;}

    // 获取当前状态名称 - 直接返回记录的状态名
    QString getCurrentStateName() const {
        return m_currentStateName;
    }

    //用于外部访问motorPos数组
    float* getMotorPositions() {
        return this->motorPos;
    }
    void start() {
        boost::msm::back::state_machine<FrontFSM>::start();
    }

private:
    ZMotionControl* m_zm;
    DMDetected* m_dm;
    QString m_currentStateName; // 存储当前状态名
};

// 实例化四个具体后端状态机
typedef FSMBackend<TakeFloorFSMFront> TakeFloorFSM;
typedef FSMBackend<TakePlateFSMFront> TakePlateFSM;
typedef FSMBackend<CoverPlateFSMFront> CoverPlateFSM;
typedef FSMBackend<CoverFloorFSMFront> CoverFloorFSM;

#endif // COVERPLATEFSM_H

//状态机在处理状态转换时，会将触发转换的事件对象传递给目标状态的on_entry方法
//而状态机在调用状态的on_entry方法时，会将自身作为FSM&参数传入
//FSMBackend<TakeFloorFSMFront> ，相当于告诉编译器用 TakeFloorFSMFront 这个类型替换模板中的 FrontFSM，生成一个专门适配该前端的后端类
//lambda 表达式[]（捕获列表）用于指定 lambda 表达式可以访问的外部变量
//lambda 表达式中()内的参数是输入参数列表，其含义与普通函数的参数列表完全一致

