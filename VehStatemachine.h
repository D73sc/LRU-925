// VehicleStateMachine.h
#ifndef VEHICLESTATEMACHINE_H
#define VEHICLESTATEMACHINE_H


#ifndef BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#endif
#ifndef BOOST_MPL_LIMIT_VECTOR_SIZE
#define BOOST_MPL_LIMIT_VECTOR_SIZE 50 //or whatever you need
#endif

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/back/tools.hpp>
#include <boost/mpl/placeholders.hpp>
#include <QObject>
#include <iostream>
#include <functional>
#include <QString>
#include <QDebug>
#include "logmgr.h"

namespace msm = boost::msm;
namespace mpl = boost::mpl;


// FSM front-end
class VehicleStateMachine_ : public QObject, public msm::front::state_machine_def<VehicleStateMachine_>
{
    Q_OBJECT
public:
    VehicleStateMachine_(QObject* parent = nullptr) : QObject(parent) {}

    // --- External control command interface ---
    std::function<void(const QString& target)> sendControlCommand;//函数包装器，无返回值，传入QString&类型数据

    // --- States ---
    struct LM0 : msm::front::state<>
    {
        static QString name() { return "LM0"; }
        template <class Event, class FSM>
        void on_entry(Event const& evt, FSM&fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name() );
        }

        template <class Event, class FSM>
        void on_exit(Event const&, FSM& fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }
    };
    struct LM1 : msm::front::state<>
    {
        static QString name() { return "LM1"; }
        template <class Event, class FSM>
        void on_entry(Event const& evt, FSM&fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }

        template <class Event, class FSM>
        void on_exit(Event const&, FSM& fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }

    };
    struct LM2 : msm::front::state<>
    {
        static QString name() { return "LM2"; }
        template <class Event, class FSM>
        void on_entry(Event const& evt, FSM&fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }

        template <class Event, class FSM>
        void on_exit(Event const&, FSM& fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }
    };
    struct LM3 : msm::front::state<>
    {
        static QString name() { return "LM3"; }
        template <class Event, class FSM>
        void on_entry(Event const& evt, FSM&fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }

        template <class Event, class FSM>
        void on_exit(Event const&, FSM& fsm)
        {
            //log
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }
    };

    struct Running : msm::front::state<>
    {
        static QString name() { return "Running"; }

        template <class Event, class FSM>
        void on_entry(Event const& ev, FSM& fsm)
        {
            //std::cout << "[Running::on_entry] Start motion, setup timeout timer\n";
            if (fsm.sendControlCommand)
            {
                QString target;
                if (typeid(ev) == typeid(EvMoveToLM0)) target = "LM0";
                else if (typeid(ev) == typeid(EvMoveToLM1)) target = "LM1";
                else if (typeid(ev) == typeid(EvMoveToLM2)) target = "LM2";
                else if (typeid(ev) == typeid(EvMoveToLM3)) target = "LM3";
                fsm.sendControlCommand(target);
            }
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }

        template <class Event, class FSM>
        void on_exit(Event const&, FSM&)
        {
//            std::cout << "[Running::on_exit] Stop timeout timer\n";
            LogMgr::getInstance()->m_logger->info("{} state {}",__FUNCTION__,typeid(this).name());
        }
    };

    struct Error : msm::front::state<> { static QString name() { return "Error"; } };

    // --- Initial State ---
    typedef LM0 initial_state;

    // --- Events ---
    struct EvMoveToLM0 {};
    struct EvMoveToLM1 {};
    struct EvMoveToLM2 {};
    struct EvMoveToLM3 {};
    struct EvArrivedToLM0 {};
    struct EvArrivedToLM1 {};
    struct EvArrivedToLM2 {};
    struct EvArrivedToLM3 {};
    struct EvError {};
    struct EvTimeout {};
    struct EvClearError {};

    // --- Action ---
    struct HandleTimeout
    {
        template <class EVT, class FSM, class Source, class Target>
        void operator()(EVT const&, FSM&, Source const&, Target const&)
        {
//            std::cout << "[Action] Motion timeout occurred\n";
            qDebug()<<"[Action] Motion timeout occurred\n";
        }
    };

    // --- Transition Table ---
    //状态转移表Row<LM0, EvMoveToLM1, Running>：起始状态，触发事件，转移到新的状态，事件触发后的动作，条件判断(为真才执行转移)
    struct transition_table : mpl::vector<
                                  msm::front::Row<LM0, EvMoveToLM1, Running>,
                                  msm::front::Row<LM0, EvMoveToLM2, Running>,
                                  msm::front::Row<LM0, EvMoveToLM3, Running>,

                                  msm::front::Row<LM1, EvMoveToLM0, Running>,
                                  msm::front::Row<LM1, EvMoveToLM2, Running>,
                                  msm::front::Row<LM1, EvMoveToLM3, Running>,

                                  msm::front::Row<LM2, EvMoveToLM0, Running>,
                                  msm::front::Row<LM2, EvMoveToLM1, Running>,
                                  msm::front::Row<LM2, EvMoveToLM3, Running>,

                                  msm::front::Row<LM3, EvMoveToLM0, Running>,
                                  msm::front::Row<LM3, EvMoveToLM1, Running>,
                                  msm::front::Row<LM3, EvMoveToLM2, Running>,

                                  msm::front::Row<Running, EvArrivedToLM0, LM0>,
                                  msm::front::Row<Running, EvArrivedToLM1, LM1>,
                                  msm::front::Row<Running, EvArrivedToLM2, LM2>,
                                  msm::front::Row<Running, EvArrivedToLM3, LM3>,

                                  msm::front::Row<Running, EvTimeout, Error, HandleTimeout>,

//                                  msm::front::Row<msm::front::none, EvError, Error>,
                                  msm::front::Row<LM0, EvError, Error>,
                                  msm::front::Row<LM1, EvError, Error>,
                                  msm::front::Row<LM2, EvError, Error>,
                                  msm::front::Row<LM3, EvError, Error>,
                                  msm::front::Row<Running, EvError, Error>,


                                  // Allow clear error and transition back to fixed location
                                  msm::front::Row<Error, EvMoveToLM0, Running>,
                                  msm::front::Row<Error, EvMoveToLM1, Running>,
                                  msm::front::Row<Error, EvMoveToLM2, Running>,
                                  msm::front::Row<Error, EvMoveToLM3, Running>
                                  > {};

signals:
    void sigStateChanged(const QString& stateName);
};

// FSM back-end
class VehicleStateMachine : public msm::back::state_machine<VehicleStateMachine_>
{
    Q_OBJECT
public:
    VehicleStateMachine(QObject* parent = nullptr) : msm::back::state_machine<VehicleStateMachine_>(), m_qobjectFSM(parent) {
        m_qobjectFSM.setParent(parent);
    }

    VehicleStateMachine_* operator->() { return &m_qobjectFSM; }
    VehicleStateMachine_& getFSMObject() { return m_qobjectFSM; }

    QString getCurrentStateName() const {
        const int id = this->current_state()[0];
        switch (id) {
        case 0: return VehicleStateMachine_::LM0::name();
        case 1: return VehicleStateMachine_::LM1::name();
        case 2: return VehicleStateMachine_::LM2::name();
        case 3: return VehicleStateMachine_::LM3::name();
        case 4: return VehicleStateMachine_::Running::name();
        case 5: return VehicleStateMachine_::Error::name();
        default: return "Unknown";
        }
    }

private:
    VehicleStateMachine_ m_qobjectFSM;
};

#endif // VEHICLESTATEMACHINE_H
