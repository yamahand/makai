#pragma once
#include "Logger.hpp"

namespace mk {

// Logger の初期化・シャットダウンを RAII で管理するガード
class LoggerGuard {
public:
    LoggerGuard() {
        Logger::init();
    }
    ~LoggerGuard() {
        Logger::shutdown();
    }

    LoggerGuard(const LoggerGuard&) = delete;
    LoggerGuard& operator=(const LoggerGuard&) = delete;
    LoggerGuard(LoggerGuard&&) = delete;
    LoggerGuard& operator=(LoggerGuard&&) = delete;
};

} // namespace mk
