#ifndef PINMONITOR_H
#define PINMONITOR_H

#include <QMainWindow>

namespace Ui {
class PinMonitor;
}

class PinMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit PinMonitor(QWidget *parent = nullptr);
    ~PinMonitor();

private:
    Ui::PinMonitor *ui;
};

#endif // PINMONITOR_H
