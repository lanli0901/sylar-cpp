#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"

int main(int argc, char** argv){
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(sylar::LogLevel::DEBUG);
    logger->addAppender(file_appender);

    // sylar::LogEvent::ptr event(new sylar::LogEvent(__FILE__, __LINE__, 0, sylar::GetThreadID(), sylar::GetFiberID(), time(0)));
    // event->getSS() << "hello sylar log";
    // logger->log(sylar::LogLevel::DEBUG, event);
    // std::cout << "hello sylar log" << std::endl;

    SYLAR_LOG_INFO(logger) << "test_macro";
    SYLAR_LOG_ERROR(logger) << "test_macro_error";

    SYLAR_LOG_FMT_DEBUG(logger, "test macro fmt error %s", "aa");

    // 使用管理logger的单例
    auto l = sylar::LoggerMgr::GetInstance()->getLogger("xx");
    // std::cout << (l == sylar::LoggerMgr::GetInstance()->getRoot()) << std::endl;
    SYLAR_LOG_INFO(l) << "xxx";
    return 0;
}