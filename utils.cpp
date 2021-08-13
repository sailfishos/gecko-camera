/*
 * Copyright (C) 2021 Open Mobile Platform LLC.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <syslog.h>
#include <cstring>
#include <iostream>

#include "utils.h"

namespace gecko {
namespace camera {
namespace internal {

class SysLogBuffer : public std::basic_streambuf<char, std::char_traits<char>>
{
public:
    explicit SysLogBuffer(std::string ident, LogLevel level)
        : logLevel(level)
        , lineLevel(kLogDebug)
    {
        strncpy(logTag, ident.c_str(), sizeof(logTag));
        logTag[sizeof(logTag) - 1] = 0;
        openlog(logTag, LOG_PID, LOG_USER);
    }

    void setLineLevel(LogLevel level)
    {
        lineLevel = level;
    }

protected:
    int sync()
    {
        if (lineLevel >= logLevel && buffer.length()) {
            syslog(syslogLevel(lineLevel), "%s", buffer.c_str());
            buffer.erase();
        }
        lineLevel = kLogDebug;
        return 0;
    }

    std::streamsize xsputn(const char *buf, std::streamsize sz)
    {
        if (lineLevel >= logLevel) {
            buffer += std::string(buf, sz);
        }
        return sz;
    }

    int overflow(int c)
    {
        if (c != EOF) {
            if (lineLevel >= logLevel) {
                buffer += static_cast<char>(c);
            }
        } else {
            sync();
        }
        return c;
    }

private:
    static int syslogLevel(LogLevel level)
    {
        switch (level) {
        case kLogError:
            return LOG_ERR;
        case kLogInfo:
            return LOG_INFO;
        case kLogDebug:
            return LOG_DEBUG;
        }
        return LOG_DEBUG;
    }

    std::string buffer;
    char logTag[32];
    LogLevel logLevel;
    LogLevel lineLevel;
};

} // namespace internal

void LogInit(std::string logTag, LogLevel logLevel)
{
    std::clog.rdbuf(new internal::SysLogBuffer(logTag, logLevel));
}

} // namespace camera
} // namespace gecko

std::ostream &operator<< (std::ostream &os, const gecko::camera::LogLevel &level)
{
    static_cast<gecko::camera::internal::SysLogBuffer *>(os.rdbuf())->setLineLevel(level);
    return os;
}
/* vim: set ts=4 et sw=4 tw=80: */
