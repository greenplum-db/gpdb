#ifndef __S3_MACROS_H__
#define __S3_MACROS_H__

#include <cstdarg>
#include <sstream>
#include <stdexcept>
#include <string>

using std::string;
using std::stringstream;

#define BUFFER_LEN 1024

extern void StringAppendPrintf(std::string *output, const char *format, ...);

#define CHECK_OR_DIE_MSG(_condition, _format, _args...)                     \
    do {                                                                    \
        if (!(_condition)) {                                                \
            std::string _error_str;                                         \
            StringAppendPrintf(&_error_str, _format, _args);                \
            std::stringstream _err_msg;                                     \
            _err_msg << _error_str << ", Function: " << __func__              \
                     << ", File: " << __FILE__ << "(" << __LINE__ << "). "; \
            throw std::runtime_error(_err_msg.str());                       \
        }                                                                   \
    } while (false)

#define CHECK_ARG_OR_DIE(_arg) \
    CHECK_OR_DIE_MSG(_arg, "Null pointer of argument: '%s'", #_arg)

#define CHECK_OR_DIE(_condition)                                             \
    do {                                                                     \
        if (!(_condition)) {                                                 \
            std::stringstream _err_msg;                                      \
            _err_msg << "Failed expression: (" << #_condition                 \
                     << "), Function: " << __func__ << ", File: " << __FILE__ \
                     << "(" << __LINE__ << "). ";                            \
            throw std::runtime_error(_err_msg.str());                        \
        }                                                                    \
    } while (false)

inline void VStringAppendPrintf(string *output, const char *format,
                                va_list argp) {
    CHECK_OR_DIE(output);
    char buffer[BUFFER_LEN];
    vsnprintf(buffer, BUFFER_LEN, format, argp);
    output->append(buffer);
}

inline void StringAppendPrintf(string *output, const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    VStringAppendPrintf(output, format, argp);
    va_end(argp);
}

#endif
