#pragma once

#include <deque>
#include <format>
#include <mutex>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// LogLevel
// ---------------------------------------------------------------------------
enum class LogLevel : uint8_t { Debug, Info, Warn, Error };

// ---------------------------------------------------------------------------
// Logger — singleton, thread-safe.
//
// Call sites use the LOG_* macros which compile to nothing in non-debug
// builds (when ENABLE_LOGGING is not defined).
//
// The render thread calls snapshot() once per frame to copy entries into
// the ImGui debug window without holding the lock during drawing.
// ---------------------------------------------------------------------------
class Logger {
public:
    static Logger& instance();

    struct Entry {
        LogLevel    level;
        std::string tag;    // filename:line
        std::string msg;
    };

    template<typename... Args>
    void log(LogLevel level, std::string_view file, int line,
             std::format_string<Args...> fmt, Args&&... args)
    {
        logImpl(level, file, line,
                std::format(fmt, std::forward<Args>(args)...));
    }

    // Copies up to MAX_ENTRIES recent entries — safe to call from render thread.
    std::deque<Entry> snapshot() const;
    void clear();

    static constexpr size_t MAX_ENTRIES = 512;

private:
    Logger() = default;
    void logImpl(LogLevel level, std::string_view file, int line,
                 std::string msg);

    mutable std::mutex mtx_;
    std::deque<Entry>  entries_;
};

// ---------------------------------------------------------------------------
// Macros — strip to nothing unless ENABLE_LOGGING is defined.
// ---------------------------------------------------------------------------
#ifdef ENABLE_LOGGING
#  define LOG_DEBUG(...) ::Logger::instance().log(::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#  define LOG_INFO(...)  ::Logger::instance().log(::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#  define LOG_WARN(...)  ::Logger::instance().log(::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#  define LOG_ERROR(...) ::Logger::instance().log(::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#else
#  define LOG_DEBUG(...) ((void)0)
#  define LOG_INFO(...)  ((void)0)
#  define LOG_WARN(...)  ((void)0)
#  define LOG_ERROR(...) ((void)0)
#endif
