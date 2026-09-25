#include "sriSensorWorker.h"
#include <QDebug>

SRISensorWorker::SRISensorWorker(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_sensorId(1)  // 默认ID
{
    m_sensorManager = new CSRISensorManager();
    m_readTimer = new QTimer(this);
    connect(m_readTimer, &QTimer::timeout, this, [this]() {
        if (m_running){
            if (m_sensorManager->GetRealtimeData(m_sensorId, sensorData)) {
                // for(int i=0;i<6;i++){
                //     QString Mystring = QString("通道%1:%2").arg(i).arg(sensorData.channels[i]);
                //     emit initializationResult(true, Mystring);
                // }
                emit sensorDataReady(&sensorData);
            }
        }
    });
}

SRISensorWorker::~SRISensorWorker()
{
    stopReadingData();
    if (m_sensorManager) {
        delete m_sensorManager;
        m_sensorManager = nullptr;
    }
}

void SRISensorWorker::initializeSensor(const QString& ip, int port)
{
    m_ipAddress = ip;
    m_port = port;

    if (m_sensorManager->sriRunSensorManager(ip.toStdString(), port)) {
        m_sensorManager->sriStartSample(m_sensorId, 1000);
        emit initializationResult(true, "六维力传感器连接成功！");
    } else {
        emit initializationResult(false, "六维力传感器连接失败！");
    }
}

void SRISensorWorker::startReadingData()
{
    if (m_sensorManager) {
        m_running = true;
        m_readTimer->start(200);
        emit initializationResult(true, "开始读取六维力传感器数据");
    } else {
        emit initializationResult(false, "传感器未初始化，请先初始化传感器");
    }
}

void SRISensorWorker::stopReadingData()
{
    m_running = false;
    m_readTimer->stop();
    if (m_sensorManager) {
        m_sensorManager->sriStopSample(m_sensorId);
    }
    emit initializationResult(true, "停止读取六维力传感器数据");
}

void SRISensorWorker::setAllSensorDataZero()
{
    if (m_sensorManager) {
        m_sensorManager->SetAllSensorDataZero();
        emit initializationResult(true, "所有传感器已调零");
    } else {
        emit initializationResult(false, "传感器未初始化");
    }
}
