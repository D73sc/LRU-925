#ifndef LIFTMONITOR_H
#define LIFTMONITOR_H

#include <QMainWindow>
#include "TCPSensor.h"
#include "Lift.h"


using namespace nsTaskMgr;

namespace Ui {
class LiftMonitor;
}

class LiftMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit LiftMonitor(ZMotionControl* zm,QWidget *parent = nullptr);
    bool IsCollsion = false;
    QTimer* IsCollsionTimer;
    ~LiftMonitor();

public slots:
    void onMotorStatusUpdated();
    void updateSixAxisSensorInfo(SixForeData* data);
    void updateSixDofPlatformUI(SixDofPlatformData& data);
    void updateDetectedDisplayUI(const DetectedDisplayData& data);
    void UpdateCollsionUI();


signals:

private:
    Ui::LiftMonitor *ui;
    ZMotionControl* m_zm;
};

#endif // LIFTMONITOR_H
