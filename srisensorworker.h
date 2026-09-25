#ifndef SRISENSORWORKER_H
#define SRISENSORWORKER_H

#include <QObject>
#include <QThread>
#include <QString>
#include "sriSensorManager.h"
#include "sriSensorDataDefine.h"
#include <QTimer>
#include <QElapsedTimer>

class SRISensorWorker : public QObject
{
    Q_OBJECT

public:
    explicit SRISensorWorker(QObject *parent = nullptr);
    ~SRISensorWorker();

public slots:
    void initializeSensor(const QString& ip, int port);
    void startReadingData();
    void stopReadingData();
    void setAllSensorDataZero();

signals:
    void sensorDataReady(ForceSensorData* sensorData);
    void initializationResult(bool success, const QString& message);

private:
    CSRISensorManager* m_sensorManager;
    QString m_ipAddress;
    int m_port;
    volatile bool m_running;
    QTimer* m_readTimer;
    QElapsedTimer m_elapsedTimer;
    BYTE m_sensorId;  // 传感器ID，默认为1
    ForceSensorData sensorData;
    float MysensorData[6];
};

#endif // SRISENSORWORKER_H
