#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include "loghandler.h"
#include "DataType.h" // 包含你的结构体定义
//新增
#include "CrashHandler.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString qss;
    QFile file(":/MyQss.qss");
    file.open(QFile::ReadOnly);
    qss = file.readAll();
    file.close();
    a.setStyleSheet(qss);
    qRegisterMetaType<nsTaskMgr::DetectedDisplayData>("nsTaskMgr::DetectedDisplayData");
    MainWindow w;
    w.show();
    LogHandler::Get().installMessageHandler();
    CrashHandler::install();//新增
    return a.exec();
}



