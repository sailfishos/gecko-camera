/*
 * Copyright (C) 2022 Open Mobile Platform LLC.
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
#include <algorithm>
#include <fstream>

#include "geckocamera-utils.h"
#include "droid-common.h"

using namespace std;
using namespace gecko::camera;

DroidRawImageFrame::~DroidRawImageFrame()
{
    LOGV("buffer: " << (void *)m_buffer);

    if (m_buffer) {
        droid_media_buffer_unlock(m_buffer);
    }
}

bool DroidRawImageFrame::map(DroidGraphicBuffer *buffer, DroidMediaBuffer *droidBuffer)
{
    const void *imageData = droid_media_buffer_lock(droidBuffer, DROID_MEDIA_BUFFER_LOCK_READ);
    if (imageData) {
        data = static_cast<const uint8_t *>(imageData);
        width = buffer->width;
        height = buffer->height;
        imageFormat = buffer->imageFormat;
        timestampUs = buffer->timestampUs;
        m_buffer = droidBuffer;

        LOGV("created " << this
             << " data=" << (const void *)data
             << " timestampUs=" << timestampUs);
        return true;
    }
    return false;
}

DroidYCbCrFrame::~DroidYCbCrFrame()
{
    LOGV("buffer: " << (void *)m_buffer);

    if (m_buffer) {
        droid_media_buffer_unlock(m_buffer);
    }
}

bool DroidYCbCrFrame::map(DroidGraphicBuffer *buffer, DroidMediaBuffer *droidBuffer)
{
    DroidMediaBufferYCbCr ycbcr;

    if (droid_media_buffer_lock_ycbcr(droidBuffer,
                DROID_MEDIA_BUFFER_LOCK_READ,
                &ycbcr)) {
        y = static_cast<const uint8_t*>(ycbcr.y);
        cb = static_cast<const uint8_t*>(ycbcr.cb);
        cr = static_cast<const uint8_t*>(ycbcr.cr);
        yStride = ycbcr.ystride;
        cStride = ycbcr.cstride;
        chromaStep = ycbcr.chroma_step;
        width = buffer->width;
        height = buffer->height;
        timestampUs = buffer->timestampUs;
        m_buffer = droidBuffer;

        LOGV("created " << this
             << " y=" << (const void *)y
             << " yStride=" << yStride
             << " cStride=" << cStride
             << " chromaStep=" << chromaStep
             << " timestampUs=" << timestampUs);

        return true;
    }
    return false;
}

DroidGraphicBuffer::DroidGraphicBuffer(
    DroidObject *parent,
    DroidMediaBuffer *buffer)
    : DroidObject(parent)
    , m_droidBuffer(buffer)
{
    width = droid_media_buffer_get_width(buffer);
    height = droid_media_buffer_get_height(buffer);
    handle = buffer;
    timestampUs = droid_media_buffer_get_timestamp(buffer) / 1000;
}

DroidGraphicBuffer::~DroidGraphicBuffer()
{
    droid_media_buffer_release(m_droidBuffer, NULL, 0);
}

shared_ptr<const YCbCrFrame> DroidGraphicBuffer::mapYCbCr()
{
    shared_ptr<DroidYCbCrFrame> ptr = make_shared<DroidYCbCrFrame>(this);
    bool success = false;

    if (m_droidBuffer && imageFormat == ImageFormat::YCbCr) {
        success = ptr->map(this, m_droidBuffer);
    }
    return success ? static_pointer_cast<YCbCrFrame>(ptr) : nullptr;
}

shared_ptr<const RawImageFrame> DroidGraphicBuffer::map()
{
    shared_ptr<DroidRawImageFrame> ptr = make_shared<DroidRawImageFrame>(this);
    bool success = false;

    if (m_droidBuffer) {
        success = ptr->map(this, m_droidBuffer);
    }
    return success ? static_pointer_cast<RawImageFrame>(ptr) : nullptr;
}

DroidGraphicBufferPool::Item::Item(DroidObject *parent, DroidMediaBuffer *buffer)
    : DroidObject(parent)
    , m_buffer(buffer)
{
}

DroidGraphicBufferPool::Item::~Item()
{
    droid_media_buffer_destroy(m_buffer);
}

std::shared_ptr<GraphicBuffer> DroidGraphicBufferPool::Item::acquire()
{
    return std::make_shared<DroidGraphicBuffer>(this, m_buffer);
}

bool DroidGraphicBufferPool::bind(DroidObject *parent, DroidMediaBuffer *buffer)
{
    // Create the new pool item and store its index+1 in buffer's user data
    m_items.push_back(make_shared<Item>(parent, buffer));
    droid_media_buffer_set_user_data(buffer, (void*)m_items.size());
    return true;
}

shared_ptr<GraphicBuffer> DroidGraphicBufferPool::acquire(DroidMediaBuffer *buffer)
{
    size_t index = (size_t)droid_media_buffer_get_user_data(buffer);
    if (index && index <= m_items.size()) {
        return m_items[index - 1]->acquire();
    }
    return nullptr;
}

void DroidGraphicBufferPool::clear()
{
    m_items.clear();
}

class DroidSystemInfoImpl : public DroidSystemInfo
{
public:
    void update()
    {
        if (!m_initialized) {
            readCpuInfo();
            m_initialized = true;
        }
    }

private:
    static void lstrip(string &s)
    {
        s.erase(s.begin(),
                find_if(s.begin(), s.end(), [](unsigned char c) {
                    return !isspace(c);
                }));
    }

    static bool startswith(string &s, string subs)
    {
        return s.find(subs) == 0;
    }

    static CpuVendor guessCpuVendor(string line) {
        size_t sep = line.find(":", 8);
        if (sep != string::npos) {
            line.erase(0, sep + 1);
            lstrip(line);
            if (startswith(line, "MT")) {
                return CpuVendor::MediaTek;
            } else if (startswith(line, "Qualcomm")) {
                return CpuVendor::Qualcomm;
            }
        }
        return CpuVendor::Unknown;
    }

    bool readCpuInfo()
    {
        string line;
        ifstream cpuinfo("/proc/cpuinfo");
        if (cpuinfo.is_open())  {
            while (getline(cpuinfo, line)) {
                if (startswith(line, "Hardware")) {
                    cpuVendor = guessCpuVendor(line);
                }
            }
            cpuinfo.close();
            return true;
        }
        return false;
    }

    bool m_initialized = false;
};

// static
DroidSystemInfo& DroidSystemInfo::get()
{
    static DroidSystemInfoImpl instance;
    instance.update();
    return instance;
}

// static
bool DroidSystemInfo::envIsSet(const char *env)
{
    const char *envValue = getenv(env);
    return envValue && strcmp(envValue, "0") && strcmp(envValue, "");
}

/* vim: set ts=4 et sw=4 tw=80: */
