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
#include <map>
#include <set>
#include <filesystem>

#include "geckocamera.h"
#include "utils.h"

namespace gecko {
namespace camera {

using namespace std;

class RootCameraManager : public CameraManager
{
public:
    RootCameraManager() {}
    ~RootCameraManager() {}

    bool init();
    int getNumberOfCameras();
    bool getCameraInfo(unsigned int num, CameraInfo &info);
    bool queryCapabilities(const string cameraId, vector<CameraCapability> &caps);
    bool openCamera(const string cameraId, shared_ptr<Camera> &camera);

private:
    bool initialized = false;
    void findCameras();

    vector<CameraInfo> cameraInfoList;
    map<const string, shared_ptr<CameraManager>> cameraIdMap;
    map<const string, shared_ptr<CameraManager>> plugins;

    shared_ptr<CameraManager> loadPlugin(string path);
};

bool RootCameraManager::init()
{
    if (!initialized) {
        LogInit("gecko-camera", getenv("GECKO_CAMERA_DEBUG") ? kLogDebug : kLogInfo);
        filesystem::directory_entry pluginDir(GECKO_CAMERA_PLUGIN_DIR);
        if (pluginDir.exists() && pluginDir.is_directory()) {
            for (const auto &entry : filesystem::directory_iterator(GECKO_CAMERA_PLUGIN_DIR)) {
                if (entry.is_regular_file()) {
                    string path = entry.path();
                    auto found = plugins.find(path);
                    if (found == plugins.end()) {
                        auto plugin = loadPlugin(path);
                        if (plugin && plugin->init()) {
                            LOGI("Initialized plugin at " << path);
                            plugins.emplace(path, plugin);
                        }
                    }
                }
            }
        }
        initialized = true;
    }
    return true;
}

int RootCameraManager::getNumberOfCameras()
{
    init();
    findCameras();
    return cameraInfoList.size();
}

bool RootCameraManager::getCameraInfo(unsigned int num, CameraInfo &info)
{
    if (num < cameraInfoList.size()) {
        info = cameraInfoList.at(num);
        return true;
    }
    return false;
}

bool RootCameraManager::queryCapabilities(
    const string cameraId,
    vector<CameraCapability> &caps)
{
    auto iter = cameraIdMap.find(cameraId);
    if (iter != cameraIdMap.end()) {
        auto plugin = iter->second;
        return plugin->queryCapabilities(cameraId, caps);
    }
    return false;
}

bool RootCameraManager::openCamera(const string cameraId, shared_ptr<Camera> &camera)
{
    auto iter = cameraIdMap.find(cameraId);
    if (iter != cameraIdMap.end()) {
        auto plugin = iter->second;
        return plugin->openCamera(cameraId, camera);
    }
    return false;
}

void RootCameraManager::findCameras()
{
    cameraInfoList.clear();
    cameraIdMap.clear();
    for (auto const& [path, plugin] : plugins) {
        for (int i = 0; i < plugin->getNumberOfCameras(); i++) {
            CameraInfo info;
            if (plugin->getCameraInfo(i, info)) {
                cameraInfoList.push_back(info);
                cameraIdMap.insert_or_assign(info.id, plugin);
            }
        }
    }
}

shared_ptr<CameraManager> RootCameraManager::loadPlugin(string path)
{
    // Clear error
    dlerror();

    void *handle = dlopen(path.c_str(), RTLD_LAZY);
    if (handle) {
        CameraManager* (*_manager)() = (CameraManager * (*)())dlsym(handle, "gecko_camera_manager");
        if (_manager) {
            CameraManager *manager = _manager();
            if (manager) {
                // Create a shared_ptr that doesn't call delete() since manager is the pointer
                // to a static object.
                return shared_ptr<CameraManager>(shared_ptr<CameraManager> {}, manager);
            }
        }
        dlclose(handle);
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
