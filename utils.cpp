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

#include "geckocamera-utils.h"

using namespace std;

namespace gecko {
namespace camera {
namespace internal {

static bool logInitialized = false;

class SysLogBuffer : public basic_streambuf<char, char_traits<char>>
{
public:
    explicit SysLogBuffer(string ident, LogLevel level)
        : logLevel(level)
        , lineLevel(LogDebug)
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
        lineLevel = LogDebug;
        return 0;
    }

    streamsize xsputn(const char *buf, streamsize sz)
    {
        if (lineLevel >= logLevel) {
            buffer += string(buf, sz);
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
        case LogError:
            return LOG_ERR;
        case LogInfo:
            return LOG_INFO;
        case LogDebug:
            return LOG_DEBUG;
        }
        return LOG_DEBUG;
    }

    string buffer;
    char logTag[32];
    LogLevel logLevel;
    LogLevel lineLevel;
};

} // namespace internal

void LogInit(string logTag, LogLevel logLevel)
{
    if (!internal::logInitialized) {
        clog.rdbuf(new internal::SysLogBuffer(logTag, logLevel));
        internal::logInitialized = true;
    }
}

} // namespace camera
} // namespace gecko

ostream &operator<< (ostream &os, const gecko::camera::LogLevel &level)
{
    static_cast<gecko::camera::internal::SysLogBuffer *>(os.rdbuf())->setLineLevel(level);
    return os;
}
/* vim: set ts=4 et sw=4 tw=80: */
