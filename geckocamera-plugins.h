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

#ifndef __GECKO_CAMERA_PLUGINS__
#define __GECKO_CAMERA_PLUGINS__

#include <vector>
#include <mutex>

namespace gecko {
namespace camera {

struct Plugin {
    std::string path;
    void *handle;
};

class PluginManager {
public:
    static PluginManager *get();
    std::vector<Plugin> listPlugins();
private:
    std::vector<Plugin> m_plugins;
    std::mutex m_mutex;
    bool m_initialized = false;
};

} // namespace camera
} // namespace gecko

#endif // __GECKO_CAMERA_PLUGINS__
/* vim: set ts=4 et sw=4 tw=80: */
