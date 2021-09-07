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

#include <cstring>
#include <thread>
#include <chrono>

#include "geckocamera.h"

using namespace std;
using namespace gecko::camera;

class DummyCamera;

class DummyCameraManager : public CameraManager
{
public:
    explicit DummyCameraManager() {}
    ~DummyCameraManager() {}

    bool init()
    {
        return true;
    }

    int getNumberOfCameras()
    {
        return 1;
    }

    bool getCameraInfo(unsigned int num, CameraInfo &info)
    {
        info.name = "Dummy camera";
        info.id = "dummy:rear";
        info.provider = "dummy";
        info.facing = GECKO_CAMERA_FACING_REAR;
        return true;
    }

    bool queryCapabilities(string cameraId,
                           vector<CameraCapability> &caps);

    bool openCamera(string cameraId, shared_ptr<Camera> &camera);
};

class DummyCameraFrame : public YCbCrFrame
{
public:
    static shared_ptr<YCbCrFrame> create(
        shared_ptr<DummyCamera> camera, unsigned int phase)
    {
        return static_pointer_cast<YCbCrFrame>(
                   make_shared<DummyCameraFrame>(camera, phase));
    }

    explicit DummyCameraFrame(shared_ptr<DummyCamera> camera, unsigned int phase);
    ~DummyCameraFrame() {}

private:
    shared_ptr<DummyCamera> m_camera;
};

class DummyCamera : public Camera, public enable_shared_from_this<DummyCamera>
{
public:
    static shared_ptr<DummyCamera> create(CameraManager *manager)
    {
        return make_shared<DummyCamera>(manager);
    }

    explicit DummyCamera(CameraManager *manager)
        : m_manager(manager)
        , m_started(false)
    {
        // Fill video frame with garbage.
        memset(m_frameData, 128, sizeof(m_frameData));
        uint8_t val = 0;
        for (size_t i = 0; i < sizeof(m_frameData); i += m_width / 10) {
            m_frameData[i] = val++;
        }
    }

    ~DummyCamera()
    {
        stopCapture();
    }

    bool getInfo(CameraInfo &info)
    {
        return m_manager->getCameraInfo(0, info);
    }

    bool startCapture(const CameraCapability &cap)
    {
        if (!m_started) {
            m_cameraThread = thread(&cameraLoop, this);
            m_started = true;
        }
        return true;
    }

    bool stopCapture()
    {
        if (m_started) {
            m_started = false;
            m_cameraThread.join();
        }
        return true;
    }

    bool captureStarted() const
    {
        return m_started;
    }

    bool queryCapabilities(vector<CameraCapability> &caps)
    {
        CameraCapability cap;

        cap.width = 320;
        cap.height = 240;
        cap.fps = 30;
        caps.push_back(cap);

        return true;
    }

    bool open()
    {
        return true;
    }

    friend class DummyCameraFrame;

private:
    CameraManager *m_manager;
    volatile bool m_started;

    void loop()
    {
        unsigned int phase = 0;
        while (m_started) {
            auto frame = DummyCameraFrame::create(shared_from_this(), phase++);
            if (cameraListener) {
                cameraListener->onCameraFrame(frame);
            }
            // Sleep 1000/30 milliseconds to produce ~30 fps.
            this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
        }
    }

    static void cameraLoop(DummyCamera *camera)
    {
        camera->loop();
    }

    thread m_cameraThread;

    static const int m_width = 320;
    static const int m_height = 240;
    static const int m_maxOffset = m_width / 10;
    uint8_t m_frameData[m_width * m_height + m_maxOffset];
};

bool DummyCameraManager::queryCapabilities(string cameraId,
                                           vector<CameraCapability> &caps)
{
    auto camera = DummyCamera::create(this);
    if (camera->open()) {
        return camera->queryCapabilities(caps);
    }
    return false;
}

bool DummyCameraManager::openCamera(string cameraId, shared_ptr<Camera> &camera)
{
    auto dummyCamera = DummyCamera::create(this);
    if (dummyCamera->open()) {
        camera = static_pointer_cast<Camera>(dummyCamera);
        return true;
    }
    return false;
}

DummyCameraFrame::DummyCameraFrame(shared_ptr<DummyCamera> camera, unsigned int phase)
    : m_camera(camera)
{
    phase %= camera->m_maxOffset;

    yStride = camera->m_width;
    cStride = (camera->m_width + 1) / 2;
    height = camera->m_height;
    chromaStep = 1;
    y = camera->m_frameData + phase;
    cb = camera->m_frameData + phase;
    cr = camera->m_frameData + phase;
    timestampUs = chrono::duration_cast<std::chrono::microseconds>(
            chrono::high_resolution_clock::now().time_since_epoch()).count();
}

static DummyCameraManager dummyCameraManager;

extern "C" __attribute__((visibility("default"))) CameraManager *gecko_camera_plugin_manager(void)
{
    return &dummyCameraManager;
}
/* vim: set ts=4 et sw=4 tw=80: */
