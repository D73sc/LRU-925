#include "pinmonitor.h"
#include "ui_pinmonitor.h"

PinMonitor::PinMonitor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::PinMonitor)
{
    ui->setupUi(this);
}

PinMonitor::~PinMonitor()
{
    delete ui;
}
