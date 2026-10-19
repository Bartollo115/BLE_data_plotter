#include "logger.hpp"

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

Logger& Logger::instance()
{
    static Logger s;
    return s;
}

static std::string_view basename(std::string_view path)
{
    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

static const char* levelStr(LogLevel l)
{
    switch (l) {
    case LogLevel::Debug: return "DBG";
    case LogLevel::Info:  return "INF";
    case LogLevel::Warn:  return "WRN";
    case LogLevel::Error: return "ERR";
    }
    return "???";
}

void Logger::logImpl(LogLevel level, std::string_view file, int line,
                     std::string msg)
{
    std::string tag = std::format("{}:{}", basename(file), line);
    std::string line_str = std::format("[{}] {:<20} {}", levelStr(level), tag, msg);

    // stdout
    std::cout << line_str << '\n';

    // MSVC output window
#ifdef _WIN32
    OutputDebugStringA((line_str + "\n").c_str());
#endif

    // Ring buffer for ImGui window
    {
        std::lock_guard lock(mtx_);
        if (entries_.size() >= MAX_ENTRIES)
            entries_.pop_front();
        entries_.push_back({ level, std::move(tag), std::move(msg) });
    }
}

std::deque<Logger::Entry> Logger::snapshot() const
{
    std::lock_guard lock(mtx_);
    return entries_;
}

void Logger::clear()
{
    std::lock_guard lock(mtx_);
    entries_.clear();
}
