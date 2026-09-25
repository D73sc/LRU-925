#ifndef LEVELMONITOR_H
#define LEVELMONITOR_H

#include <QMainWindow>
#include "SerialSensor.h"
#include "zmotioncontrol.h"
#include "DataType.h"

using namespace nsTaskMgr;

namespace Ui {
class LevelMonitor;
}

class LevelMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit LevelMonitor(ZMotionControl* zm,QWidget *parent = nullptr);
    QTimer* CurrentTimer;
    ~LevelMonitor();

public slots:
    void UpdateForceData(const QVariantMap &data);
    void UpdateAngleData(const QVariantMap &data);
    void onMotorStatusUpdated();
    void CurrentUpdate(float current0, float current1, float current2, float current3);
    void onLevelSymbolChanged(LevelSymbol symbol);

private slots:


private:
    Ui::LevelMonitor *ui;
    ZMotionControl* m_zm;
};

#endif // LEVELMONITOR_H
