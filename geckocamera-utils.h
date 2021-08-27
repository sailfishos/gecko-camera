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

#ifndef __GECKO_CAMERA_UTILS_H__
#define __GECKO_CAMERA_UTILS_H__

#include <iostream>

namespace gecko {
namespace camera {

enum LogLevel {
    LogDebug = 0,
    LogInfo,
    LogError
};

void LogInit(std::string logTag, enum LogLevel logLevel);

#ifndef LOG_TOPIC
#define LOG_TOPIC "main"
#endif /* LOG_TOPIC */

#ifndef NDEBUG
#define LOG(l, x) \
    std::clog << l << LOG_TOPIC " " << __FUNCTION__ << ":" << __LINE__ << " -- " \
              << x << std::endl
#else
#define LOG(l, x) do {} while (0)
#endif

#ifdef VERBOSE_LOGGING
#define LOGV(x) LOG(gecko::camera::LogDebug, x)
#else
#define LOGV(x) do {} while (0)
#endif

#define LOGD(x) LOG(gecko::camera::LogDebug, x)
#define LOGI(x) LOG(gecko::camera::LogInfo, x)
#define LOGE(x) LOG(gecko::camera::LogError, x)

} // namespace camera
} // namespace gecko

std::ostream &operator<< (std::ostream &os, const gecko::camera::LogLevel &level);

#endif /* __GECKO_CAMERA_UTILS_H__ */
/* vim: set ts=4 et sw=4 tw=80: */
