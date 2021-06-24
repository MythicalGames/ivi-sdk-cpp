#ifndef __IVI_UTIL_H__
#define __IVI_UTIL_H__

#ifndef IVI_STATIC_ASSERT
#define IVI_STATIC_ASSERT(expr) static_assert(expr, #expr)
#endif // IVI_STATIC_ASSERT

/* 
 * Logging is enabled by default, set to 0 and recompile lib to disable, or to the desired level
 * via IVI_LOGGING_LEVEL.
 * 
 * Suggested level is 2 (critical errors & warnings) if you use the default logging functionality.
 * 
 * Default is 4 when unset.
 * 
 * Additionally, the default logging function will output to standard out, but can be switched to
 * standard error by setting IVI_LOGGING_CERR to nonzero.
 * 
 * You can also replace the logging function at runtime by setting the IVILogImpl and/or IVILogStreamImpl
 * function pointers.  In that case you may want to turn off IVI_LOGGING_PREFIX as well.
 */
#ifndef IVI_LOGGING_LEVEL
#define IVI_LOGGING_LEVEL 4
#endif // IVI_LOGGING_LEVEL

#ifndef IVI_LOGGING_CERR
#define IVI_LOGGING_CERR 0
#endif // IVI_LOGGING_CERR

// Control if the log message should include LogPrefix() below
#ifndef IVI_LOGGING_PREFIX
#define IVI_LOGGING_PREFIX 1
#endif // IVI_LOGGING_PREFIX

// Control whether IVI_CHECK will call std::exit(EXIT_FAILURE)
#ifndef IVI_ENABLE_EXIT_ON_FAIL_CHECK
#define IVI_ENABLE_EXIT_ON_FAIL_CHECK 1
#endif // IVI_ENABLE_EXIT_ON_FAIL_CHECK

#if IVI_ENABLE_EXIT_ON_FAIL_CHECK
#include <cstdlib>
#define IVI_EXIT_FAILURE() std::exit(EXIT_FAILURE)
#else
#define IVI_EXIT_FAILURE()
#endif // IVI_ENABLE_EXIT_ON_FAIL_CHECK

#if IVI_LOGGING_LEVEL > 0
    #include "ivi-types.h"
    namespace ivi
    {
        enum class LogLevel : int
        {
            CRITICAL    = 1 << 1,
            WARNING     = 1 << 2,
            RPC_FAIL    = 1 << 3,
            INFO        = 1 << 4,
            VERBOSE     = 1 << 5,
            NTRACE      = 1 << 6,   // network tracing
            DTRACE      = 1 << 7    // debug tracing
        };

        using LogFunc           = void(*)(LogLevel, const string&);
        using LogStreamFunc     = void(*)(LogLevel, ostringstream& oss);

        // Set this at runtime to your own callback, default just prints to standard out
        extern LogFunc          IVILogImpl;  
        extern LogStreamFunc    IVILogStreamImpl;

        // Appends the current timestamp and LogLevel string
        void LogPrefix(LogLevel logLevel, ostringstream& oss);

        template<typename... Args>
        void IVILog(const LogLevel logLevel, Args&&... args)
        {
            ostringstream oss;
    #if IVI_LOGGING_PREFIX
            LogPrefix(logLevel, oss);
    #endif // IVI_LOGGING_PREFIX
            // C++11 non-recursive in-order parameter pack expansion, can be (might need to be) changed in C++17 or C++20
            auto _(initializer_list<int> { ((oss << forward<Args>(args)), 0)... });
            (void)_; // make clang happpy
            IVILogStreamImpl(logLevel, oss);
        }
    } // namespace ivi

    #define IVI_LOG(level, ...) ::ivi::IVILog(ivi::LogLevel::level, __VA_ARGS__)

    #define IVI_LOG_CRITICAL(...) IVI_LOG(CRITICAL, __VA_ARGS__)

    #if IVI_LOGGING_LEVEL >= 2
        #define IVI_LOG_WARNING(...) IVI_LOG(WARNING, __VA_ARGS__)
    #else
        #define IVI_LOG_WARNING(...)
    #endif // IVI_LOGGING_LEVEL >= 2

    #if IVI_LOGGING_LEVEL >= 3
        #define IVI_LOG_RPC_FAIL(...) IVI_LOG(RPC_FAIL, __VA_ARGS__)
    #else
        #define IVI_LOG_RPC_FAIL(...)
    #endif // IVI_LOGGING_LEVEL >= 3


    #if IVI_LOGGING_LEVEL >= 4
        #define IVI_LOG_INFO(...) IVI_LOG(INFO, __VA_ARGS__)
    #else
        #define IVI_LOG_INFO(...)
    #endif // IVI_LOGGING_LEVEL >= 4

    #if IVI_LOGGING_LEVEL >= 5
        #define IVI_LOG_VERBOSE(...) IVI_LOG(VERBOSE, __VA_ARGS__)
    #else
        #define IVI_LOG_VERBOSE(...)
    #endif // IVI_LOGGING_LEVEL >= 5

    #if IVI_LOGGING_LEVEL >= 6
        #define IVI_LOG_NTRACE(...) IVI_LOG(VERBOSE, __VA_ARGS__)
    #else
        #define IVI_LOG_NTRACE(...)
    #endif // IVI_LOGGING_LEVEL >= 6


    #if IVI_LOGGING_LEVEL >= 7
        #define IVI_LOG_DTRACE(...) IVI_LOG(DTRACE, __VA_ARGS__)
        #define IVI_LOG_SCOPE(x) ::ivi::LogScope __logScope__(x);
    #else
        #define IVI_LOG_DTRACE(...)
        #define IVI_LOG_SCOPE(...)
    #endif // IVI_LOGGING_LEVEL >= 7

    namespace ivi
    {
        struct LogScope
        {
            string str;
            template<typename C>
            inline LogScope(C scopeStr)
                : str(scopeStr)
            {
                IVI_LOG_DTRACE(str, " BEGIN");
            }
            inline ~LogScope()
            {
                IVI_LOG_DTRACE(str, " END");
            }
        };
    } // namespace ivi

#else // IVI_LOGGING_LEVEL == 0

    #define IVI_LOG(level, ...)
    #define IVI_LOG_CRITICAL(...)
    #define IVI_LOG_WARNING(...)
    #define IVI_LOG_RPC_FAIL(...)
    #define IVI_LOG_INFO(...)
    #define IVI_LOG_VERBOSE(...)
    #define IVI_LOG_NTRACE(...)
    #define IVI_LOG_DTRACE(...)
    #define IVI_LOG_SCOPE(...)

#endif // IVI_LOGGING_LEVEL

#define IVI_CHECK(expr) if(!(expr)) { IVI_LOG_CRITICAL("CHECK FAILED: ", #expr); IVI_EXIT_FAILURE(); }
#define IVI_LOG_FUNC() IVI_LOG_SCOPE(__func__)
#define IVI_LOG_FUNC_TRIVIAL() IVI_LOG_DTRACE(__func__)

#endif //__IVI_UTIL_H__
