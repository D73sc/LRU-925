#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_dahengTwoCams_qt_vs.h"
#include "vision_source/CDeviceProcess.h"
#include <QDir>
#include <QDateTime>
#include <QFileDialog>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <QElapsedTimer>
#include <opencv2/videoio.hpp>
#include "LRUdatadef.h"
#include <QThread>
#include <QMutex>
#define  DEVICE_CONTS 2               //最多允许同时操作2台设备

class CameraWorker;

class dahengTwoCams_qt_vs : public QMainWindow
{
    Q_OBJECT

public:
    dahengTwoCams_qt_vs(QWidget *parent = Q_NULLPTR);
    ~dahengTwoCams_qt_vs();

private:
    Ui::dahengTwoCams_qt_vsClass ui;

	//初始化界面UI
	void __InitUI(CGXFeatureControlPointer objFeatureControlPtr);

	//更新界面
	void __UpdateUI(CDeviceProcess* pDeviceProcess);
    // 过滤掉非 USB 相机（GigE 等）
    void __FilterUsbOnly(GxIAPICPP::gxdeviceinfo_vector& vec);

	//关闭窗口
	void closeEvent(QCloseEvent *event);

	//初始化API
	void __InitCGXAPI();

	GxIAPICPP::gxdeviceinfo_vector m_vectorDeviceInfo;          // 枚举到的设备信息
	CDeviceProcess* m_pDeviceProcess[DEVICE_CONTS];             // 设备处理类对象
	CDeviceProcess* m_pDeviceProcessCurrent;                    // 当前设备处理类对象
	std::map<int, CDeviceProcess*> m_mapDeviceInformation;      // 用于存储设备处理类
	bool     m_bIsSnapSpeed;                                    // 是否支持采集速度级别
	bool     m_bIsColorFilter;                                  // 是否支持Bayer格式
	double   m_dShutterMax;                                     // 曝光时间最大值
	double   m_dShutterMin;                                     // 曝光时间最小值
	double   m_dGainMax;                                        // 增益最大值
	double   m_dGainMin;                                        // 增益最小值
	int64_t  m_nSnapSpeedMax;                                   // 采集速度级别最大值
	int64_t  m_nSnapSpeedMin;                                   // 采集速度级别最小值
	int      m_nDeviceListCurrent;                              // 当前设备列表序号
	int      m_nCurrentBalanceAutoWhiteSel;                     // 记录白平衡的值
	gxstring m_strBalanceWhiteAuto;                             // 当前自动白平衡
    int camera0_exposureTime = 200000;
    int camera1_exposureTime = 200000;

    // 新增：逻辑索引(0,1,...) -> 枚举索引(在 m_vectorDeviceInfo 里的下标)
    std::vector<int> m_sortedIndexBySN;
    bool m_bVisionStarted = false;

public:
	double		m_dEditShutter;
	double		m_dEditGain;
	int64_t		m_nEditSnapSpeed;
	bool		m_bCheckShowDevice;

    QLabel		*m_pLLabelStaBar;
    QLabel		*m_pRLabelStaBar;
	QTimer		*m_Timer;

	void time_update();
	void on_Btn_refreshDeviceList_clicked();
	void on_Btn_OpenDevice_clicked();
	void on_Btn_CloseDevice_clicked();
	void on_Btn_StartCapture_clicked();
	void on_Btn_StopCapture_clicked();
	void on_Cb_DeviceList_currentIndexChanged(int nIndex);
	void on_lE_Shutter_editingFinished();

    // 新函数：通过设备索引号打开指定设备
    bool OpenDeviceByIndex(int deviceIndex);

    // 新函数：通过设备索引号开始指定设备的采集
    void StartCaptureByIndex(int deviceIndex);
    // 一键式启动
    bool visionprocess();
    // 获取最新帧
    cv::Mat getlatestframe(int deviceIndex);

    void setShutter_us(int deviceIndex, double dExposureUs);
    QMutex m_cameraMutex;
public slots:
    void onParamsReceived(const LRUInnerParams &params);

signals:
    void dahengUpdateUI(QString str);
private slots:
    void on_closecamera_clicked();
};

