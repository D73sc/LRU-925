#ifndef AGVMONITOR_H
#define AGVMONITOR_H

#include <QMainWindow>
#include "SCVehicle.h"
#include "SerialSensor.h"

namespace Ui {
class AGVMonitor;
}

class AGVMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit AGVMonitor(QWidget *parent = nullptr);
    ~AGVMonitor();

public slots:
    void UpdateVehStatus(VehicleSts veh_sts);
    void UpdateDistanceData(const QVariantMap &data);
    void UpdateBatterydata(const QVariantMap &data);

private slots:


private:
     Ui::AGVMonitor *ui;

};

#endif // AGVMONITOR_H
