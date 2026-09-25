#include "agvmonitor.h"
#include "ui_agvmonitor.h"

AGVMonitor::AGVMonitor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AGVMonitor)
{
    ui->setupUi(this);
}

AGVMonitor::~AGVMonitor()
{
    delete ui;
}


void AGVMonitor::UpdateVehStatus(VehicleSts veh_sts)
{
    QString Style = "border-radius: 7px; border: 2px solid black; background-color: white;";

    ui->lineEdit_batt_level->setText(
        QString::number(veh_sts.battery_level * 100.0f, 'f', 0) + "%");
    ui->lineEdit_agv_blocked->setText(QString::number(veh_sts.blocked));
    ui->lineEdit_block_reason->setText(BlockReason2Qstr(veh_sts.block_reason));
    ui->lineEdit_taskStatus->setText(TaskStatus2Qstr(veh_sts.task_status));
    ui->lineEdit_emergency->setText(QString::number(veh_sts.emergency));
    ui->lineEdit_soft_emc->setText(QString::number(veh_sts.soft_emc));
    ui->lineEdit_current_station->setText(veh_sts.current_station);
    ui->lineEdit_brake->setText(QString::number(veh_sts.brake));
    ui->lineEdit_task_type->setText(QString::number(veh_sts.task_type));
    ui->lineEdit_target_dist->setText(QString::number(veh_sts.target_dist));
    ui->lineEdit_driver_emc->setText(QString::number(veh_sts.driver_emc));
    ui->lineEdit_electric->setText(QString::number(veh_sts.electric));
    ui->lineEdit_loadmap_status->setText(LoadmapStatus2Qstr(veh_sts.loadmap_status));
    ui->lineEdit_reloc_status->setText(RelocStatus2Qstr(veh_sts.reloc_status));
    if(veh_sts.locked){
        ui->lbl_handle->setStyleSheet(Style.replace("white","green"));
    }
    else{
        ui->lbl_handle->setStyleSheet(Style);
    }


    for (auto var : veh_sts.finished_path)
    {
        ui->textEdit_Info->append("Finished path:" + var);
    }
    for (auto var : veh_sts.unfinished_path)
    {
        ui->textEdit_Info->append("Unfinished path:" + var);
    }

    for (auto error : veh_sts.errors)
    {
        ui->textEdit_Info->append(QString::number(error.code) + ":" + error.desc);
    }
}


void AGVMonitor::UpdateDistanceData(const QVariantMap &data)
{
    foreach(const QString &key, data.keys()) {
        float value = data[key].toFloat();
        if (key == "Distance0") {
            ui->Distance0->setText(QString::number(value, 'f', 1));
        } else if (key == "Distance1") {
            ui->Distance1->setText(QString::number(value, 'f', 1));
        } else if (key == "Distance2") {
            ui->Distance2->setText(QString::number(value, 'f', 1));
        } else if (key == "Distance3") {
            ui->Distance3->setText(QString::number(value, 'f', 1));
        }
    }
}

void AGVMonitor::UpdateBatterydata(const QVariantMap &data)
{
    foreach(const QString &key, data.keys()) {
        float value = data[key].toFloat();
        if (key == "remainingmAh") {
            ui->remainingmAh->setText(QString::number(value * 100.0f, 'f', 0) + "%");
        } else if (key == "packTotalVoltage") {
            ui->packTotalVoltage->setText(QString::number(value, 'f', 1));
        } else if (key == "packTotalCurrent") {
            ui->packTotalCurrent->setText(QString::number(value, 'f', 1));
        }
    }
}

