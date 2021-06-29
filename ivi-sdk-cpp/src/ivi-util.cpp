#include "ivi/ivi-util.h"

#if IVI_LOGGING_LEVEL

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace ivi
{
    static void DefaultLog(LogLevel /*logLevel*/, const string& str)
    {
#if IVI_LOGGING_CERR
        std::cerr
#else 
        std::cout
#endif // IVI_LOGGING_CERR
            << str;
    }

    // c++11 defect fixed in c++14, usage of enums as std::map keys
    static std::unordered_map<int, string> LogLevelPrefix{
        { static_cast<int>(LogLevel::DTRACE),     "[DTRACE]   "},
        { static_cast<int>(LogLevel::NTRACE),     "[NTRACE]   "},
        { static_cast<int>(LogLevel::VERBOSE),    "[VERBOSE]  "},
        { static_cast<int>(LogLevel::INFO),       "[INFO]     "},
        { static_cast<int>(LogLevel::RPC_FAIL),   "[RPC_FAIL] "},
        { static_cast<int>(LogLevel::WARNING),    "[WARNING]  "},
        { static_cast<int>(LogLevel::CRITICAL),   "[CRITICAL] "}
    };

    static void CurrentTimeString(std::ostringstream& oss)
    {
        using namespace std::chrono;
        auto now(system_clock::now());
        auto ms( duration_cast<milliseconds>(now.time_since_epoch()) % 1000 );
        std::time_t nowtime(system_clock::to_time_t(now));

#if _MSC_VER
        std::tm gmt;
        gmtime_s(&gmt, &nowtime);
#else
        std::tm gmt;
        gmtime_r(&nowtime, &gmt);
#endif
        
        oss << '[' << std::put_time(&gmt, "%Y-%m-%d %H:%M:%S") << '.'
            << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    }

    static void DefaultLogStream(LogLevel logLevel, ostringstream& oss)
    {
        oss << std::endl;
        IVILogImpl(logLevel, oss.str());
    }

    void LogPrefix(LogLevel logLevel, ostringstream& oss)
    {
        CurrentTimeString(oss);
        oss << LogLevelPrefix[static_cast<int>(logLevel)];
    }

    IVI_SDK_API LogFunc         IVILogImpl      (&DefaultLog);
    IVI_SDK_API LogStreamFunc   IVILogStreamImpl(&DefaultLogStream);
}

#endif // IVI_LOGGING_ENABLED
