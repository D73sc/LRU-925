#ifndef NEWFORM_H
#define NEWFORM_H

#include <QMainWindow>
#include "zmotioncontrol.h"

namespace Ui {
class NewForm;
}

class NewForm : public QMainWindow
{
    Q_OBJECT

public:
    explicit NewForm(QWidget *parent = nullptr);
    // 获取复选框状态
    bool getIsRealGridChecked() const;
    bool getIsRealWorldCoord() const;
    bool getIsJointChecked(int jointIndex) const;
    void bindAllSliders();
    void bindAllCheckBoxes();
    ~NewForm();

signals:
    void sigSetModelRealTimeShow(bool value);

private slots:
    void slotUpdateGlobalConfig();

    void on_sliderJoint1_valueChanged(int value);

    void on_sliderJoint2_valueChanged(int value);

    void on_sliderJoint3_valueChanged(int value);

    void on_sliderJoint4_valueChanged(int value);

    void on_UpdateTimer_Out();
private:
    Ui::NewForm *ui;
    ZMotionControl *zm;
    QTimer *UpdateTimer;

    float m_lastPos1; // 缓存纵移上一次位置
    float m_lastPos2; // 缓存横移上一次位置
    float m_lastPos3; // 缓存手指上一次位置
    float m_lastPos4; // 缓存托举上一次位置
};

#endif // NEWFORM_H
