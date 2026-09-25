#include "logmgr.h"

LogMgr* LogMgr::m_instance = nullptr;
std::mutex LogMgr::m_mutex;

LogMgr::LogMgr():m_logger(nullptr)
{
    spdlog::init_thread_pool(8192, 1);
    spdlog::set_level(spdlog::level::trace);
    //console
    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt >();
    stdout_sink->set_level(spdlog::level::trace);
    // stdout_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [thread %t] %v");
    stdout_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    //file,rotating
    // auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("mylog.txt", 1024*1024*10,3);
    // rotating_sink->set_level(spdlog::level::debug);
    // rotating_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] [%g:%#] [Fcn:%!] %v");
    //file,daily
    auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("log", 23, 59);
    daily_sink->set_level(spdlog::level::trace);
    daily_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] [%g:%#] [Fcn:%!] %v");

    std::vector<spdlog::sink_ptr> sinks {stdout_sink, daily_sink};
    //create logger
//    m_logger = std::make_shared<spdlog::async_logger>("logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    m_logger = std::make_shared<spdlog::logger>("logger", sinks.begin(), sinks.end());//, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    spdlog::register_logger(m_logger);

}

LogMgr* LogMgr:: getInstance()
{
    if (m_instance == nullptr)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_instance == nullptr)
        {
            m_instance = new LogMgr();
        }
    }
    return m_instance;
}
