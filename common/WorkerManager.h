#ifndef WORKERMANAGER_H
#define WORKERMANAGER_H
#include "Worker.h"

// 工作线程管理器
class WorkerManager : public QObject {
    Q_OBJECT
public:
    // 创建指定数量的工作线程
    explicit WorkerManager(int threadCount,QObject* parent = nullptr);


    // 将处理器分配到指定Worker
    void assignHandler(int workerIndex, EventHandler* handler);

private:
    std::vector<std::unique_ptr<Worker>> m_workers;
};

#endif // WORKERMANAGER_H

/*

// 具体处理器实现
// 网络处理器（模拟网络请求）
class NetworkHandler : public EventHandler {
public:
    void handleEvent() override {
        qDebug() << "[NetworkHandler] 正在下载数据...";
    }
};

// 数据库处理器（模拟数据库操作）
class DatabaseHandler : public EventHandler {
public:
    void handleEvent() override {
        qDebug() << "[DatabaseHandler] 正在写入记录...";
    }
};

// 日志处理器（模拟日志记录）
class LogHandler : public EventHandler {
public:
    void handleEvent() override {
        qDebug() << "[LogHandler] 正在记录日志...";
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // 创建处理器实例
    NetworkHandler networkHandler;
    DatabaseHandler dbHandler;
    LogHandler logHandler;

    // 创建工作管理器（根据CPU核心数创建Worker）
    WorkerManager manager;

    // 将处理器分配到不同Worker
    manager.assignHandler(0, &networkHandler);  // Worker 0 处理网络
    manager.assignHandler(1, &dbHandler);       // Worker 1 处理数据库
    manager.assignHandler(1, &logHandler);      // Worker 1 同时处理日志

    return a.exec();
}


*/
