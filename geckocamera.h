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

#ifndef __GECKO_CAMERA__
#define __GECKO_CAMERA__

#include <sys/types.h>

#include <string>
#include <vector>
#include <memory>

namespace gecko {
namespace camera {

enum CameraFacing {
    GECKO_CAMERA_FACING_FRONT = 0,
    GECKO_CAMERA_FACING_REAR
};

struct CameraCapability {
    unsigned int width;
    unsigned int height;
    unsigned int fps;
};

struct CameraInfo {
    std::string id;
    std::string name;
    std::string provider;
    CameraFacing facing;
    unsigned int mountAngle;
};

struct CameraFrame {
    const void *y;
    const void *cb;
    const void *cr;
    size_t yStride;
    size_t cStride;
    size_t chromaStep;
    uint64_t timestampMs;
    size_t height;
};

class CameraListener
{
public:
    virtual ~CameraListener() = default;
    virtual void onCameraFrame(std::shared_ptr<const CameraFrame> frame) = 0;
    virtual void onCameraError(std::string errorDescription) = 0;
};

class Camera
{
public:
    virtual ~Camera() = default;
    virtual bool getInfo(CameraInfo &info) = 0;
    virtual bool startCapture(const CameraCapability &cap) = 0;
    virtual bool stopCapture() = 0;
    virtual bool captureStarted() const = 0;

    virtual void setListener(CameraListener *listener)
    {
        cameraListener = listener;
    }

protected:
    CameraListener *cameraListener = nullptr;
};

class CameraManager
{
public:
    virtual ~CameraManager() = default;
    virtual bool init() = 0;
    virtual int getNumberOfCameras() = 0;
    virtual bool getCameraInfo(unsigned int num, CameraInfo &info) = 0;
    virtual bool queryCapabilities(const std::string cameraId,
                                   std::vector<CameraCapability> &caps) = 0;
    virtual bool openCamera(const std::string cameraId, std::shared_ptr<Camera> &camera) = 0;
};

}
}

/* Constructor for root CameraManager */
extern "C" __attribute__((visibility("default")))
gecko::camera::CameraManager *gecko_camera_manager(void);

#endif /* __GECKO_CAMERA__ */
/* vim: set ts=4 et sw=4 tw=80: */
