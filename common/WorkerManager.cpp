#include "WorkerManager.h"

// 创建指定数量的工作线程
WorkerManager:: WorkerManager(int threadCount,QObject* parent): QObject(parent)
{
    for (int i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(std::make_unique<Worker>());
    }
}

// 将处理器分配到指定Worker
void WorkerManager::assignHandler(int workerIndex, EventHandler* handler)
{
    if (workerIndex >= 0 && workerIndex < m_workers.size()) {
        m_workers[workerIndex]->registerHandler(handler);
    }
}
