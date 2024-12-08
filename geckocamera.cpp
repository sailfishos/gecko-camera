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
#include <cstring>
#include <map>

#include "geckocamera.h"
#include "geckocamera-plugins.h"
#include "geckocamera-utils.h"

namespace gecko {
namespace camera {

using namespace std;

class RootCameraManager : public CameraManager
{
public:
    RootCameraManager() {}
    ~RootCameraManager() {}

    bool init() override;
    int getNumberOfCameras() override;
    bool getCameraInfo(unsigned int num, CameraInfo &info) override;
    bool queryCapabilities(const string &cameraId, vector<CameraCapability> &caps) override;
    bool openCamera(const string &cameraId, shared_ptr<Camera> &camera) override;

private:
    void findCameras();
    shared_ptr<CameraManager> loadPlugin(Plugin &plugin);

    mutex m_mutex;
    bool m_initialized = false;
    vector<CameraInfo> m_cameraInfoList;
    map<const string, shared_ptr<CameraManager>> m_cameraIdMap;
    map<const string, shared_ptr<CameraManager>> m_plugins;
};

bool RootCameraManager::init()
{
    scoped_lock lock(m_mutex);
    if (!m_initialized) {
        for (auto plugin : PluginManager::get()->listPlugins()) {
            auto manager = loadPlugin(plugin);
            if (manager && manager->init()) {
                m_plugins.emplace(plugin.path, manager);
            }
        }
        m_initialized = true;
    }
    return true;
}

int RootCameraManager::getNumberOfCameras()
{
    init();
    findCameras();
    return m_cameraInfoList.size();
}

bool RootCameraManager::getCameraInfo(unsigned int num, CameraInfo &info)
{
    if (num < m_cameraInfoList.size()) {
        info = m_cameraInfoList.at(num);
        return true;
    }
    return false;
}

bool RootCameraManager::queryCapabilities(
    const string &cameraId,
    vector<CameraCapability> &caps)
{
    auto iter = m_cameraIdMap.find(cameraId);
    if (iter != m_cameraIdMap.end()) {
        auto plugin = iter->second;
        return plugin->queryCapabilities(cameraId, caps);
    }
    return false;
}

bool RootCameraManager::openCamera(const string &cameraId, shared_ptr<Camera> &camera)
{
    auto iter = m_cameraIdMap.find(cameraId);
    if (iter != m_cameraIdMap.end()) {
        auto plugin = iter->second;
        return plugin->openCamera(cameraId, camera);
    }
    return false;
}

void RootCameraManager::findCameras()
{
    scoped_lock lock(m_mutex);
    m_cameraInfoList.clear();
    m_cameraIdMap.clear();
    for (auto const& [path, plugin] : m_plugins) {
        for (int i = 0; i < plugin->getNumberOfCameras(); i++) {
            CameraInfo info;
            if (plugin->getCameraInfo(i, info)) {
                m_cameraInfoList.push_back(info);
                m_cameraIdMap.insert_or_assign(info.id, plugin);
            }
        }
    }
}

shared_ptr<CameraManager> RootCameraManager::loadPlugin(Plugin &plugin)
{
    if (plugin.handle) {
        CameraManager* (*_manager)() = (CameraManager * (*)())dlsym(plugin.handle, "gecko_camera_plugin_manager");
        if (_manager) {
            CameraManager *manager = _manager();
            if (manager) {
                // Create a shared_ptr that doesn't call delete() since manager is the pointer
                // to a static object.
                return shared_ptr<CameraManager>(shared_ptr<CameraManager> {}, manager);
            }
        }
    }
    return nullptr;
}

static RootCameraManager cameraRootManager;

} // namespace camera
} // namespace gecko

extern "C" __attribute__((visibility("default")))
gecko::camera::CameraManager *gecko_camera_manager(void)
{
    gecko::camera::cameraRootManager.init();
    return &gecko::camera::cameraRootManager;
}

/* vim: set ts=4 et sw=4 tw=80: */
