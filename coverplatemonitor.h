#ifndef COVERPLATEMONITOR_H
#define COVERPLATEMONITOR_H

#include <QMainWindow>
#include "DataType.h"
#include "zmotioncontrol.h"

using namespace nsTaskMgr;

namespace Ui {
class CoverPlateMonitor;
}

class CoverPlateMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit CoverPlateMonitor(ZMotionControl* zm,QWidget *parent = nullptr);
    ~CoverPlateMonitor();

public slots:
    void onMotorStatusUpdated();
    void onCoverPlateSymbolChanged(CoverPlateSymbol symbol);
    void resetCoverPlateSymbol();

private:
    Ui::CoverPlateMonitor *ui;
    ZMotionControl* m_zm;
};

#endif // COVERPLATEMONITOR_H
