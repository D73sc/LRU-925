#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QHeaderView>
#include <QTableView>
#include <QAbstractItemView>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include "DataType.h"


//Qwidget 提升为 QCHartView
//数据可视化
#include <QtCharts/QChartView>
using namespace QtCharts;  // 让编译器识别QChartView


using namespace nsTaskMgr;
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    //数据库单例对象
    static DatabaseManager* getInstance() {
        static DatabaseManager instance;
        return &instance;
    }

    //数据可视化
    void generateSensorChart(QChartView* chartView, const QString& selectedMainType, QWidget* parentWidget);

    QMap<QString, double> getSystemAverageDuration();

    void generateAverageDurationChart(QChartView* chartView, QWidget* parentWidget);

    //数据库初始化
    bool init(const QString& host, const QString& dbName,
              const QString& user, const QString& password) {
        m_db = QSqlDatabase::addDatabase("QMYSQL");
        m_db.setHostName(host);
        m_db.setDatabaseName(dbName);
        m_db.setUserName(user);
        m_db.setPassword(password);

        if (!m_db.open()) {
            qCritical() << "数据库连接失败:" << m_db.lastError().text();
            return false;
        }

        // 创建数据表
        createTables();
        return true;
    }


    // 断开数据库连接
    void close() {
        if (m_db.isOpen()) {
            m_db.close();
        }
    }


    // 获取表模型，可以附加筛选条件
    QSqlTableModel* getTableModel(const QString& tableName, const QString& filterCondition = "", QObject* parent = nullptr) {
        if (!m_db.isOpen() || tableName.isEmpty()) return nullptr;

        QSqlTableModel* model = new QSqlTableModel(parent, m_db);//数据库绑定
        model->setTable(tableName);//绑定数据库的具体数据表
        model->setEditStrategy(QSqlTableModel::OnManualSubmit);

        // 设置排序
        int idCol = model->fieldIndex("ID");
        if (idCol != -1) {
            model->setSort(idCol, Qt::DescendingOrder);
        }

        // 设置筛选条件
        if (!filterCondition.isEmpty()) {
            model->setFilter(filterCondition);
        }

        // 执行查询，返回的是所有符合筛选条件的数据(和表头完整对应的数据)
        if (!model->select()) {
            QMessageBox::warning(nullptr, "警告", model->lastError().text());
            delete model;
            return nullptr;
        }

        return model;
    }


    // 根据ID删除记录
    bool deleteRecordById(const QString& tableName, int id) {
        if (!m_db.isOpen() || tableName.isEmpty() || id <= 0) return false;
        return deleteRecord(tableName, QString("ID = %1").arg(id));
    }


    // 在模型中添加新行并自动填充时间字段
    bool addRowToModel(QSqlTableModel* model, const QString& tableName) {
        if (!model || tableName.isEmpty()) return false;

        int newRow = 0;
        if (!model->insertRow(newRow)) {
            qWarning() << "插入行失败:" << model->lastError().text();
            return false;
        }

        // 自动填充时间字段
        QString timeField;
        if (tableName == "task_status") {
            timeField = "创建时间";
        } else if (tableName == "sensor_data") {
            timeField = "采集时间";
        } else if (tableName == "error_log") {
            timeField = "发生时间";
        }

        if (!timeField.isEmpty()) {
            int timeCol = model->fieldIndex(timeField);
            if (timeCol != -1) {
                model->setData(model->index(newRow, timeCol),
                               QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
            }
        }
        return true;
    }


    // 提交模型更改
    bool submitModelChanges(QSqlTableModel* model) {
        if (!model) return false;
        return model->submitAll();
    }


    // 记录任务状态
    void logTaskStatus(const eTaskStatus& status) {
        if (!m_db.isOpen()) return;

        QSqlQuery query;
        query.prepare("INSERT INTO task_status "
                      "(`系统名称`, `任务类型`, `任务状态`, `任务详情`, `运行时间`) "
                      "VALUES (:system_name, :task_type, :task_status, :task_detail, :run_duration)");

        // 绑定参数
        query.bindValue(":system_name", SubSystemToString(status.sys));
        query.bindValue(":task_type", TaskTypeToString(status.taskinfo.TaskType));
        query.bindValue(":task_status", TaskStateToString(status.state));
        query.bindValue(":task_detail", status.info);
        query.bindValue(":run_duration", status.duration);

        if (!query.exec()) {
            qWarning() << "任务状态记录失败:" << query.lastError().text();
        }
    }


    // 记录传感器数据
    void logSensorData(const QString& sensorType, const QVariant& singleData) {
        if (!m_db.isOpen()){
            return;
        }


        bool isNumber = false;
        double dataValue = singleData.toDouble(&isNumber);
        if (!isNumber) {
            qWarning() << "传感器数据格式错误：仅支持数值类型" << singleData;
            return;
        }

        QSqlQuery query(m_db);
        query.prepare("INSERT INTO sensor_data "
                      "(`传感器类型`, `传感器数据`, `采集时间`) "
                      "VALUES (:type, :data, :time)");

        query.bindValue(":type", sensorType);
        query.bindValue(":data", dataValue);
        query.bindValue(":time", QDateTime::currentDateTime());

        if (!query.exec()) {
            qWarning() << "传感器数据记录失败:" << query.lastError().text();
        }
    }


    // 记录错误信息
    void logError(const QString& subsystem, const QString& errorInfo) {
        if (!m_db.isOpen()) return;

        QSqlQuery query(m_db);
        query.prepare("INSERT INTO error_log "
                      "(`系统名称`, `错误详情`, `发生时间`) "
                      "VALUES (:system_name, :error_detail, :occur_time)");

        query.bindValue(":system_name", subsystem);
        query.bindValue(":error_detail", errorInfo);
        query.bindValue(":occur_time", QDateTime::currentDateTime());

        if (!query.exec()) {
            qWarning() << "错误日志记录失败:" << query.lastError().text();
        }
    }

    // 删除MySQL中的表数据
    bool deleteRecord(const QString& tableName, const QString& condition) {
        if (!m_db.isOpen() || condition.isEmpty()) return false;

        QSqlQuery query(m_db);
        QString queryStr = QString("DELETE FROM %1 WHERE %2").arg(tableName).arg(condition);

        return query.exec(queryStr);
    }


    // 获取表结构
    QStringList getTableFields(const QString& tableName) {
        QStringList fields;
        if (!m_db.isOpen()) return fields;

        QSqlRecord record = m_db.record(tableName);
        for (int i = 0; i < record.count(); ++i) {
            fields << record.fieldName(i);
        }
        return fields;
    }


    //仅可视化最新的100条折线数据
    // 获取传感器时间序列数据，用于可视化折线图
    QList<QPair<QDateTime, double>> getSensorTimeSeriesData(const QString& tableName, const QString& sensorType, int limit) {
        QList<QPair<QDateTime, double>> timeSeriesData;
        if (!m_db.isOpen() || sensorType.isEmpty()) return timeSeriesData;

        QSqlQuery query(m_db);
        QString queryStr = QString(
                               "SELECT 采集时间, 传感器数据 FROM %1 "
                               "WHERE 传感器类型 = :sensorType "
                               "ORDER BY id DESC "//按照自增索引倒序排列配合limit，获取最新100条数据
                               "LIMIT :limit"
                               ).arg(tableName);

        query.prepare(queryStr);
        query.bindValue(":sensorType", sensorType);
        query.bindValue(":limit", limit);

        if (query.exec()) {
            while (query.next()) {
                QDateTime time = query.value(0).toDateTime();
                if (!time.isValid()) {
                    qWarning() << "无效时间：" << query.value(0).toString();
                    continue;
                }

                QString dataStr = query.value(1).toString().trimmed();
                bool ok = false;
                double value = dataStr.toDouble(&ok);

                if (ok) {
                    timeSeriesData.append(QPair<QDateTime, double>(time, value));
                } else {
                    qWarning() << "数据转换失败：传感器类型=" << sensorType
                               << "，数据=" << dataStr;
                }
            }

            // 反转列表，倒序的最新100条数据转换为顺序的最新100条数据
            std::reverse(timeSeriesData.begin(), timeSeriesData.end());

        } else {
            qWarning() << "传感器数据查询失败：" << query.lastError().text();
        }

        return timeSeriesData;
    }



   //对外提供数据库的调用接口
   QSqlDatabase getDatabase(){
       return m_db;
    }


private:
    QSqlDatabase m_db;

    DatabaseManager() = default;
    ~DatabaseManager() {
        if (m_db.isOpen()) {
            m_db.close();
        }
    }

    //创建数据库中的数据表(已通过命令行创建可以忽略)
    void createTables() {
        QSqlQuery query(m_db);

        // 任务状态表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS task_status (
                ID INT AUTO_INCREMENT PRIMARY KEY,
                系统名称 VARCHAR(50) NOT NULL,
                任务类型 VARCHAR(50) NOT NULL,
                任务状态 VARCHAR(50) NOT NULL,
                任务详情 TEXT,
                运行时间 DOUBLE NOT NULL
            )
        )");

        // 传感器数据表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS sensor_data (
                ID INT AUTO_INCREMENT PRIMARY KEY,
                传感器类型 VARCHAR(50) NOT NULL,
                传感器数据 TEXT NOT NULL,
                采集时间 DATETIME NOT NULL
            )
        )");

        // 错误日志表
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS error_log (
                ID INT AUTO_INCREMENT PRIMARY KEY,
                系统名称 VARCHAR(50) NOT NULL,
                错误详情 TEXT NOT NULL,
                发生时间 DATETIME NOT NULL
            )
        )");
    }

    // 获取传感器子类型列表
    QStringList getSubSensorTypes(const QString& mainType){

        if(mainType == "AGV系统"){
            return {"激光测距传感器-0", "激光测距传感器-1", "激光测距传感器-2", "激光测距传感器-3"};
        }

        else if(mainType == "调平系统"){
            return {"电流-电机0", "电流-电机1", "电流-电机2","电流-电机3","倾角传感器-X", "倾角传感器-Y"};
        }

        else if(mainType == "一级升降系统"){//后面还可以添加激光传感器的数据
            return {"电流-电机4"};
        }

        else if(mainType == "取盖板系统"){
            return {"电流-电机5","电流-电机6","电流-电机7","电流-电机8","电流-电机9","电流-电机10","电流-电机11"};
        }

        else if(mainType == "二级升降系统"){
            return {"电流-电机12","六维力传感器-FX", "六维力传感器-FY", "六维力传感器-FZ",
                    "六维力传感器-MX", "六维力传感器-MY", "六维力传感器-MZ"};
        }

        else if(mainType == "拔销系统"){//待定
            return {};
        }

        return {};
    }
};


#endif // DATABASEMANAGER_H
