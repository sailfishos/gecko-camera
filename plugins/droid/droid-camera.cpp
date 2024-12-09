/*
 * Copyright (C) 2021-2022 Open Mobile Platform LLC.
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

#include <map>
#include <cstring>
#include <sstream>
#include <iostream>
#include <mutex>

#include <droidmedia.h>
#include <droidmediacamera.h>

#include "geckocamera.h"
#include "droid-common.h"

#define LOG_TOPIC "droid-camera"
#include "geckocamera-utils.h"

using namespace std;
using namespace gecko::camera;

class DroidCamera;
class DroidCameraParams;
class DroidCameraGraphicBuffer;

struct DroidCameraItem {
    CameraInfo info;
    vector<CameraCapability> caps;
    weak_ptr<DroidCamera> runningInstance;
};

class DroidCameraManager : public CameraManager
{
public:
    explicit DroidCameraManager() {}
    ~DroidCameraManager() {}

    bool init();
    int getNumberOfCameras();
    bool getCameraInfo(unsigned int num, CameraInfo &info);
    bool queryCapabilities(const string &cameraId,
                           vector<CameraCapability> &caps);
    bool openCamera(const string &cameraId, shared_ptr<Camera> &camera);

    bool getCaptureAccess(shared_ptr<DroidCamera>, bool exclusive);

private:
    bool initialized = false;
    vector<DroidCameraItem> cameraList;
    bool findCameras();
    int cameraIndexById(const string &cameraId) const;
    mutex managerLock;
};

class DroidCameraYCbCrFrame : public YCbCrFrame
{
public:
    explicit DroidCameraYCbCrFrame();
    bool map(DroidCameraGraphicBuffer *buffer, const DroidMediaBufferYCbCr &tmpl, const uint8_t *addr);
private:
    shared_ptr<DroidCameraGraphicBuffer> buffer;
};

class DroidCameraGraphicBuffer
    : public GraphicBuffer
    , public enable_shared_from_this<DroidCameraGraphicBuffer>
{
public:
    explicit DroidCameraGraphicBuffer(
        DroidCamera *camera,
        DroidMediaCameraRecordingData *data);

    virtual shared_ptr<const YCbCrFrame> mapYCbCr() override;
    virtual shared_ptr<const RawImageFrame> map() override;

    ~DroidCameraGraphicBuffer();
    friend class DroidCameraYCbCrFrame;

private:
    shared_ptr<DroidCamera> camera;
    DroidMediaCameraRecordingData *recordingData;
};

class DroidCamera : public Camera, public enable_shared_from_this<DroidCamera>
{
public:
    static shared_ptr<DroidCamera> create(DroidCameraManager *manager, int num)
    {
        return make_shared<DroidCamera>(manager, num);
    }
    explicit DroidCamera(DroidCameraManager *manager, int cameraNumber);
    ~DroidCamera()
    {
        stopCapture();
    };

    int getNumber();
    bool getInfo(CameraInfo &info);
    bool startCapture(const CameraCapability &cap);
    bool stopCapture();
    bool captureStarted() const;

    bool queryCapabilities(vector<CameraCapability> &caps);
    bool open();

private:
    int cameraNumber;
    DroidCameraManager *manager;
    DroidMediaCamera *handle;
    mutex cameraLock;
    DroidGraphicBufferPool m_bufferPool;

    bool started;
    bool exclusiveAccess;

    bool openUnlocked();
    void closeUnlocked();

    shared_ptr<DroidCameraParams> currentParameters;
    bool getParameters(shared_ptr<DroidCameraParams> &params);
    bool applyParameters();

    static void error_cb(void *user, int arg);
    static void video_frame_cb(void *user, DroidMediaCameraRecordingData *);
    // Recorder queue callbacks
    static void buffers_released_cb(void *user);
    static bool buffer_created_cb(void *user, DroidMediaBuffer *);
    static bool frame_available_cb(void *user, DroidMediaBuffer *);
    // Preview queue callbacks
    static void preview_buffers_released_cb(void *user);
    static bool preview_buffer_created_cb(void *user, DroidMediaBuffer *);
    static bool preview_frame_available_cb(void *user, DroidMediaBuffer *);

    friend class DroidCameraGraphicBuffer;
};

std::ostream &operator<<(std::ostream &os, DroidCamera *camera)
{
    CameraInfo info;
    camera->getInfo(info);
    os << " (" << info.name << " 0x" << hex << (unsigned long)camera << dec << ") ";
    return os;
}

class DroidCameraParams
{
public:
    static shared_ptr<DroidCameraParams> createFromString(string params)
    {
        return make_shared<DroidCameraParams>(params);
    }
    explicit DroidCameraParams(string inp);
    ~DroidCameraParams() {};
    string getValue(const string &key);
    vector<string> getValues(const string &key);
    bool setValue(const string &key, const string &value);
    bool setCapability(CameraCapability cap);
    string toString() const;
    CameraCapability currentCapability;
    DroidMediaBufferYCbCr ycbcrTemplate;

private:
    map<string, string> params;
};

bool DroidCameraManager::init()
{
    if (!initialized) {
        initialized = droid_media_init() && findCameras();
    }
    return initialized;
}

bool DroidCameraManager::findCameras()
{
    if (!cameraList.size()) {
        for (int i = 0; i < droid_media_camera_get_number_of_cameras(); i++) {
            CameraInfo info;
            DroidMediaCameraInfo droidInfo;
            if (droid_media_camera_get_info(&droidInfo, i)) {
                if (droidInfo.facing == DROID_MEDIA_CAMERA_FACING_FRONT) {
                    info.name = "Droid front camera";
                    info.id = "droid:front:" + to_string(i);
                    info.facing = GECKO_CAMERA_FACING_FRONT;
                } else {
                    info.name = "Droid rear camera";
                    info.id = "droid:rear:" + to_string(i);
                    info.facing = GECKO_CAMERA_FACING_REAR;
                }
                info.provider = "droid";
                info.mountAngle = droidInfo.orientation;
                cameraList.push_back(DroidCameraItem{info});
            }
        }
    }
    return cameraList.size() != 0;
}

int DroidCameraManager::getNumberOfCameras()
{
    return cameraList.size();
}

bool DroidCameraManager::getCameraInfo(unsigned int num, CameraInfo &info)
{
    if (num < cameraList.size()) {
        info = cameraList.at(num).info;
        return true;
    }
    return false;
}

int DroidCameraManager::cameraIndexById(const string &cameraId) const
{
    int num = 0;
    for (auto entry : cameraList) {
        if (cameraId == entry.info.id) {
            return num;
        }
        num++;
    }
    return -1;
}

bool DroidCameraManager::queryCapabilities(
    const string &cameraId,
    vector<CameraCapability> &caps)
{
    int num = cameraIndexById(cameraId);
    if (num >= 0) {
        DroidCameraItem &entry = cameraList.at(num);
        if (entry.caps.size()) {
            caps = entry.caps;
            return true;
        } else {
            auto droidCamera = DroidCamera::create(this, num);
            if (droidCamera->open() && droidCamera->queryCapabilities(caps)) {
                entry.caps = caps;
                return true;
            }
        }
    }
    return false;
}

bool DroidCameraManager::openCamera(const string &cameraId, shared_ptr<Camera> &camera)
{
    int num = cameraIndexById(cameraId);
    if (num >= 0) {
        auto droidCamera = DroidCamera::create(this, num);
        if (droidCamera->open()) {
            camera = static_pointer_cast<Camera>(droidCamera);
            return true;
        }
    }
    return false;
}

bool DroidCameraManager::getCaptureAccess(shared_ptr<DroidCamera> camera, bool exclusive)
{
    scoped_lock lock(managerLock);

    if (exclusive) {
        // Stop all other cameras
        for (unsigned int i = 0; i < cameraList.size(); i++) {
            DroidCameraItem &entry = cameraList.at(i);
            shared_ptr<DroidCamera> runningCamera = entry.runningInstance.lock();
            if (runningCamera && runningCamera != camera) {
                runningCamera->stopCapture();
            }
            entry.runningInstance.reset();
        }
    }

    int num = camera->getNumber();
    if (num >= 0 && num < (int)cameraList.size()) {
        DroidCameraItem &entry = cameraList.at(num);
        shared_ptr<DroidCamera> runningCamera = entry.runningInstance.lock();
        if (runningCamera && runningCamera != camera) {
            runningCamera->stopCapture();
        }
        entry.runningInstance = weak_ptr<DroidCamera>(camera);
        return true;
    }
    return false;
}

DroidCamera::DroidCamera(DroidCameraManager *manager, int cameraNumber)
    : cameraNumber(cameraNumber)
    , manager(manager)
    , handle(nullptr)
    , started(false)
    , exclusiveAccess(false)
{
}

int DroidCamera::getNumber()
{
    return cameraNumber;
}

bool DroidCamera::getInfo(CameraInfo &info)
{
    return manager->getCameraInfo(cameraNumber, info);
}

bool DroidCamera::open()
{
    scoped_lock lock(cameraLock);
    return openUnlocked();
}

bool DroidCamera::openUnlocked()
{
    if (handle) {
        return true;
    }

    LOGI(this << "exclusive access: " << exclusiveAccess);

    if (!manager->getCaptureAccess(shared_from_this(), exclusiveAccess)) {
        return false;
    }

    handle = droid_media_camera_connect(cameraNumber);
    if (handle) {
        DroidMediaCameraCallbacks camera_cb;
        DroidMediaBufferQueue *queue;

        // Set preview callbacks and release the frame immediately on
        // frame_available(). This will stop log pollution with warnings
        // from droidmedia.
        queue = droid_media_camera_get_buffer_queue(handle);
        if (queue) {
            DroidMediaBufferQueueCallbacks buffer_cb;
            memset(&buffer_cb, 0, sizeof(buffer_cb));
            buffer_cb.buffers_released = DroidCamera::preview_buffers_released_cb;
            buffer_cb.frame_available = DroidCamera::preview_frame_available_cb;
            buffer_cb.buffer_created = DroidCamera::preview_buffer_created_cb;
            droid_media_buffer_queue_set_callbacks(queue, &buffer_cb, this);
        }

        queue = droid_media_camera_get_recording_buffer_queue(handle);
        if (queue) {
            DroidMediaBufferQueueCallbacks buffer_cb;
            memset(&buffer_cb, 0, sizeof(buffer_cb));
            buffer_cb.buffers_released = DroidCamera::buffers_released_cb;
            buffer_cb.frame_available = DroidCamera::frame_available_cb;
            buffer_cb.buffer_created = DroidCamera::buffer_created_cb;
            droid_media_buffer_queue_set_callbacks(queue, &buffer_cb, this);
        }

        memset(&camera_cb, 0, sizeof(camera_cb));
        camera_cb.error_cb = DroidCamera::error_cb;
        if (!queue) {
            // Capture with camera callback if buffer queue is not supported
            camera_cb.video_frame_cb = DroidCamera::video_frame_cb;
        }
        droid_media_camera_set_callbacks(handle, &camera_cb, this);
        return true;
    }

    // HAL may not support multi camera feature. Let's close other cameras
    // and try again.
    if (!exclusiveAccess) {
        exclusiveAccess = true;
        return openUnlocked();
    }

    LOGE(this << "Error connecting the camera");
    return false;
}

bool DroidCamera::getParameters(shared_ptr<DroidCameraParams> &params)
{
    if (!currentParameters.get()) {
        char *cParams = droid_media_camera_get_parameters(handle);
        if (cParams) {
            currentParameters = DroidCameraParams::createFromString(cParams);
            free(cParams);
        } else {
            LOGE(this << "Error");
            return false;
        }
    }
    params = currentParameters;
    return true;
}

bool DroidCamera::applyParameters(void)
{
    if (currentParameters.get()) {
        string applyParams = currentParameters->toString();
        LOGD(applyParams);
        return droid_media_camera_set_parameters(handle, applyParams.c_str());
    }
    return false;
}

bool DroidCamera::queryCapabilities(vector<CameraCapability> &caps)
{
    if (open()) {
        shared_ptr<DroidCameraParams> params;
        if (getParameters(params)) {
            for (string res : params->getValues("video-size-values")) {
                CameraCapability cap;
                int width, height;

                if (2 != sscanf(res.c_str(), "%dx%d", &width, &height)) {
                    return false;
                }

                LOGD(this << "supports pixel mode " << width << "x" << height);

                cap.width = width;
                cap.height = height;
                // FIXME: Is fps fixed?
                cap.fps = 30;
                caps.push_back(cap);
            }
            return true;
        }
    }
    LOGE(this << "Error");
    return false;
}

void DroidCamera::closeUnlocked()
{
    LOGI(this);

    if (handle) {
        if (started) {
            droid_media_camera_stop_recording(handle);
            droid_media_camera_stop_preview(handle);
            started = false;
        }
        droid_media_camera_disconnect(handle);
        handle = nullptr;
    }
}

bool DroidCamera::startCapture(const CameraCapability &cap)
{
    scoped_lock lock(cameraLock);

    LOGI(this);

    if (!openUnlocked()) {
        LOGE(this << "Cannot reopen the device");
        return false;
    }

    if (!started) {
        if (droid_media_camera_lock(handle)) {
            shared_ptr<DroidCameraParams> params;
            if (!getParameters(params) || !params->setCapability(cap)) {
                goto err_unlock;
            }

            if (!applyParameters()) {
                goto err_unlock;
            }

            if (!droid_media_camera_start_preview(handle)) {
                goto err_unlock;
            }

            if (!droid_media_camera_start_recording(handle)) {
                droid_media_camera_stop_preview(handle);
                goto err_unlock;
            }
            started = true;
        }
    }
    return true;

err_unlock:
    LOGE(this << "Failed to start capture");
    closeUnlocked();
    return false;
}

bool DroidCamera::stopCapture()
{
    scoped_lock lock(cameraLock);

    LOGI(this);
    closeUnlocked();
    return true;
}

bool DroidCamera::captureStarted() const
{
    return started;
}

void DroidCamera::error_cb(void *user, int arg)
{
    DroidCamera *camera = (DroidCamera *)user;
    camera->cameraListener->onCameraError(to_string(arg));
}

void DroidCamera::video_frame_cb(void *user, DroidMediaCameraRecordingData *data)
{
    DroidCamera *camera = (DroidCamera *)user;
    // Always create the buffer even if the listener is not set
    shared_ptr<DroidCameraGraphicBuffer> buffer = make_shared<DroidCameraGraphicBuffer>(camera, data);
    if (camera->cameraListener) {
        camera->cameraListener->onCameraFrame(buffer);
    }
}

void DroidCamera::buffers_released_cb(void *user)
{
    DroidCamera *camera = (DroidCamera *)user;
    camera->m_bufferPool.clear();
}

bool DroidCamera::buffer_created_cb(void *user, DroidMediaBuffer *buffer)
{
    DroidCamera *camera = (DroidCamera *)user;
    return camera->m_bufferPool.bind(nullptr, buffer);
}

bool DroidCamera::frame_available_cb(void *user, DroidMediaBuffer *droidBuffer)
{
    DroidCamera *camera = (DroidCamera *)user;

    if (droidBuffer && camera->cameraListener) {
        shared_ptr<GraphicBuffer> buffer = camera->m_bufferPool.acquire(droidBuffer);
        camera->cameraListener->onCameraFrame(buffer);
        return true;
    }
    // Tell droidmedia to release the buffer.
    return false;
}

void DroidCamera::preview_buffers_released_cb(void *user)
{
}

bool DroidCamera::preview_buffer_created_cb(void *user, DroidMediaBuffer *buffer)
{
    // Nothing to do yet
    return true;
}

bool DroidCamera::preview_frame_available_cb(void *user, DroidMediaBuffer *droidBuffer)
{
    // It is currently not used. However preview queue can be a source of
    // native buffers for video encoder to avoid converting color format
    // back and forth.
    droid_media_buffer_release(droidBuffer, NULL, 0);
    return true;
}

DroidCameraGraphicBuffer::DroidCameraGraphicBuffer(
    DroidCamera* camera,
    DroidMediaCameraRecordingData *data)
    : camera(camera->shared_from_this())
    , recordingData(data)
{
    width = camera->currentParameters->currentCapability.width;
    height = camera->currentParameters->currentCapability.height;
    timestampUs = droid_media_camera_recording_frame_get_timestamp(data) / 1000;
}

bool DroidCameraYCbCrFrame::map(DroidCameraGraphicBuffer *buffer, const DroidMediaBufferYCbCr &tmpl, const uint8_t *addr)
{
    this->buffer = buffer->shared_from_this();
    y = addr;
    cb = addr + (off_t)tmpl.cb;
    cr = addr + (off_t)tmpl.cr;
    yStride = tmpl.ystride;
    cStride = tmpl.cstride;
    chromaStep = tmpl.chroma_step;
    width = buffer->width;
    height = buffer->height;
    timestampUs = buffer->timestampUs;

    LOGV("created " << this
         << " y=" << (const void *)y
         << " yStride=" << yStride
         << " cStride=" << cStride
         << " chromaStep=" << chromaStep
         << " timestampUs=" << timestampUs);

    return true;
}

DroidCameraYCbCrFrame::DroidCameraYCbCrFrame()
    : buffer(nullptr)
{
}

shared_ptr<const YCbCrFrame> DroidCameraGraphicBuffer::mapYCbCr()
{
    shared_ptr<DroidCameraYCbCrFrame> ptr = make_shared<DroidCameraYCbCrFrame>();
    bool success = false;

    success = ptr->map(this, camera->currentParameters->ycbcrTemplate,
        static_cast<const uint8_t *>(droid_media_camera_recording_frame_get_data(recordingData)));

    return success ? static_pointer_cast<YCbCrFrame>(ptr) : nullptr;
}

shared_ptr<const RawImageFrame> DroidCameraGraphicBuffer::map()
{
    return nullptr;
}

DroidCameraGraphicBuffer::~DroidCameraGraphicBuffer()
{
    LOGV(this << " release");
    droid_media_camera_release_recording_frame(camera->handle, recordingData);
}

DroidCameraParams::DroidCameraParams(string inp)
{
    size_t pos = 0, nextPos;
    const string delimiter = ";";

    LOGD(inp);

    while ((nextPos = inp.find(delimiter, pos)) != string::npos) {
        string token = inp.substr(pos, nextPos - pos);
        size_t eqsPos = token.find("=");
        string key = token.substr(0, eqsPos);
        string value = token.substr(eqsPos + 1);
        params.insert_or_assign(key, value);
        // skip delimiter
        pos = nextPos + 1;
    }
}

string DroidCameraParams::getValue(const string &key)
{
    auto val = params.find(key);
    if (val == params.end()) {
        return string();
    } else {
        return val->second;
    }
}

vector<string> DroidCameraParams::getValues(const string &key)
{
    string valStr = getValue(key);
    vector<string> vals;
    size_t pos = 0, nextPos;
    string delimiter = ",";

    while ((nextPos = valStr.find(delimiter, pos)) != string::npos) {
        string token = valStr.substr(pos, nextPos - pos);
        vals.push_back(token);
        pos = nextPos + 1;
    }
    return vals;
}

string DroidCameraParams::toString() const
{
    ostringstream buffer;
    for (auto const& [key, val] : params) {
        buffer << key << "=" << val << ";";
    }
    return buffer.str();
}

bool DroidCameraParams::setValue(const string &key, const string &value)
{
    auto it = params.find(key);
    if (it != params.end()) {
        it->second = value;
        LOGD(key << "=" << value);
        return true;
    }
    return false;
}

bool DroidCameraParams::setCapability(CameraCapability cap)
{
    string videoFormat = getValue("video-frame-format");

#define _ALIGN_SIZE(sz, align) (((sz) + (align) - 1) & ~((align) - 1))

    // Create a template for frame format. The parameters below are possibly
    // hardware-dependent and can be read from some ini file if needed.
    if (videoFormat == "yuv420sp") {
        // Inoi R7 produces QOMX_COLOR_FormatYUV420PackedSemiPlanar32m
        unsigned int stride_w = _ALIGN_SIZE(cap.width, 128);
        unsigned int stride_h = _ALIGN_SIZE(cap.height, 32);
        ycbcrTemplate.cb = (void *)(stride_w * stride_h);
        ycbcrTemplate.cr = (void *)(stride_w * stride_h + 1);
        ycbcrTemplate.ystride = stride_w;
        ycbcrTemplate.cstride = stride_w;
        ycbcrTemplate.chroma_step = 2;
    } else {
        // Default is I420
        ycbcrTemplate.cr = (void *)(cap.width * cap.height);
        ycbcrTemplate.cb = (char *)ycbcrTemplate.cr + (cap.width * cap.height) / 4;
        ycbcrTemplate.ystride = cap.width;
        ycbcrTemplate.cstride = cap.width / 2;
        ycbcrTemplate.chroma_step = 1;
    }
    ycbcrTemplate.y = 0;

#undef _ALIGN_SIZE

    ostringstream buffer;
    currentCapability = cap;
    buffer << cap.width << "x" << cap.height;
    return setValue("video-size", buffer.str());
}

static DroidCameraManager droidCameraManager;

extern "C" __attribute__((visibility("default"))) CameraManager *gecko_camera_plugin_manager(void)
{
    return &droidCameraManager;
}

/* vim: set ts=4 et sw=4 tw=80: */
