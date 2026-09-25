#ifndef LRUMONITOR_H
#define LRUMONITOR_H

#include <QMainWindow>
#include "LRU.h"

namespace Ui {
class lrumonitor;
}

class lrumonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit lrumonitor(QWidget *parent = nullptr);
    ~lrumonitor();

    // 视觉数据接口
    float m_GapWidth = 0.0f;
    float m_AxisPos = 0.0f;
    float m_GapWidth1 = 0.0f;
    float m_GapAngle = 0.0f;

    void UpdateLRU(int mytype);

private:
    Ui::lrumonitor *ui;

    //光学模块类
    LRU *m_lru;
    QOpenGLWidget* m_glWidget;
    lrumonitor *monitor;


private slots:
    //LRU数字孪生模块
    void on_btn_LoadLRU_clicked();
    void on_btn_MonitorStart_clicked();
};

#endif // LRUMONITOR_H
