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

#ifndef __GECKOCAMERA_DROID_COMMON__
#define __GECKOCAMERA_DROID_COMMON__

#include <memory>
#include <vector>
#include <droidmedia.h>

#include "geckocamera.h"

namespace gecko {
namespace camera {

class DroidGraphicBuffer;

class DroidObject : public std::enable_shared_from_this<DroidObject>
{
public:
    DroidObject(DroidObject *parent = nullptr)
        : m_parent(parent ? parent->shared_from_this() : nullptr)
    {
    }

private:
    std::shared_ptr<DroidObject> m_parent;
};


class DroidRawImageFrame : public RawImageFrame, public DroidObject
{
public:
    explicit DroidRawImageFrame(DroidObject *parent)
        : DroidObject(parent)
    {
    }

    ~DroidRawImageFrame();

    bool map(DroidGraphicBuffer *buffer, DroidMediaBuffer *droidBuffer);

private:
    DroidMediaBuffer *m_buffer = nullptr;
};


class DroidYCbCrFrame : public YCbCrFrame, public DroidObject
{
public:
    explicit DroidYCbCrFrame(DroidObject *parent)
        : DroidObject(parent)
    {
    }

    ~DroidYCbCrFrame();

    bool map(DroidGraphicBuffer *buffer, DroidMediaBuffer *droidBuffer);

private:
    DroidMediaBuffer *m_buffer = nullptr;
};


class DroidGraphicBuffer : public GraphicBuffer, public DroidObject
{
public:
    static std::shared_ptr<DroidGraphicBuffer> create(
        DroidObject *parent,
        DroidMediaBuffer *buffer)
    {
        return std::make_shared<DroidGraphicBuffer>(parent, buffer);
    }

    explicit DroidGraphicBuffer(DroidObject *parent, DroidMediaBuffer *buffer);

    ~DroidGraphicBuffer();

    virtual std::shared_ptr<const YCbCrFrame> mapYCbCr() override;
    virtual std::shared_ptr<const RawImageFrame> map() override;

private:
    DroidMediaBuffer *m_droidBuffer;
};


class DroidGraphicBufferPool
{
public:
    std::shared_ptr<GraphicBuffer> acquire(DroidMediaBuffer *buffer);
    bool bind(DroidObject *parent, DroidMediaBuffer *buffer);
    void clear();

private:
    class Item : public DroidObject
    {
    public:
        Item(DroidObject *parent, DroidMediaBuffer *buffer);
        ~Item();
        std::shared_ptr<GraphicBuffer> acquire();

    private:
        DroidMediaBuffer *m_buffer;
    };

    std::vector<std::shared_ptr<Item>> m_items;
};

} // namespace camera
} // namespace gecko

#endif // __GECKOCAMERA_DROID_COMMON___
/* vim: set ts=4 et sw=4 tw=80: */
