#ifndef LOGMGR_H
#define LOGMGR_H
#include <mutex>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE //为了使用源码函数行号等符号用的
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"


class LogMgr
{
public:
    LogMgr();
    static LogMgr* getInstance();//getInstance()获取类的唯一实例
    std::shared_ptr<spdlog::logger> m_logger;

private:
    static LogMgr* m_instance;//m_instance保存类的唯一实例
    static std::mutex m_mutex;
};

#endif // LOGMGR_H
