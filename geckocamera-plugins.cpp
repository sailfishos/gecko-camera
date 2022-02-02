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

#include <dlfcn.h>
#include <vector>
#include <filesystem>

#include "geckocamera-plugins.h"
#include "geckocamera-utils.h"

namespace gecko {
namespace camera {

using namespace std;

std::vector<Plugin> PluginManager::listPlugins()
{
    scoped_lock lock(m_mutex);
    if (!m_initialized) {
        LogInit("gecko-camera", getenv("GECKO_CAMERA_DEBUG") ? LogDebug : LogInfo);
        filesystem::directory_entry pluginDir(GECKO_CAMERA_PLUGIN_DIR);
        if (pluginDir.exists() && pluginDir.is_directory()) {
            for (const auto &entry : filesystem::directory_iterator(GECKO_CAMERA_PLUGIN_DIR)) {
                if (entry.is_regular_file()) {
                    string path = entry.path();
                    // Clear error
                    dlerror();
                    void *handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
                    if (handle) {
                        m_plugins.push_back(Plugin{path, handle});
                    }
                }
            }
        }
        m_initialized = true;
    }
    return m_plugins;
}

static PluginManager pluginManager;

PluginManager *PluginManager::get()
{
    return &pluginManager;
}

} // namespace camera
} // namespace gecko
/* vim: set ts=4 et sw=4 tw=80: */
