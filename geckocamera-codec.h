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

#ifndef __GECKOCAMERA_CODEC__
#define __GECKOCAMERA_CODEC__

#include <sys/types.h>

#include <string>
#include <vector>
#include <memory>

#include "geckocamera.h"

namespace gecko {
namespace codec {

enum CodecType {
    VideoCodecVP8,
    VideoCodecVP9,
    VideoCodecH264,
    VideoCodecUnknown
};

enum FrameType {
    KeyFrame,
    DeltaFrame
};

struct VideoEncoderMetadata {
    CodecType codecType;
    int width;
    int height;
    int stride;
    int sliceHeight;
    int bitrate;
    int framerate;
};

class VideoEncoderListener
{
public:
    virtual ~VideoEncoderListener() = default;
    virtual void onEncodedFrame(uint8_t *data,
                                size_t size,
                                uint64_t timestampUs,
                                FrameType frameType) = 0;
    virtual void onEncoderError(std::string errorDescription) = 0;
};

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;
    virtual bool init(VideoEncoderMetadata metadata) = 0;
    virtual bool encode(std::shared_ptr<const gecko::camera::YCbCrFrame> frame,
                        bool forceSync) = 0;

    void setListener(VideoEncoderListener *listener)
    {
        m_encoderListener = listener;
    }

protected:
    VideoEncoderListener *m_encoderListener = nullptr;
};

class VideoDecoderListener
{
public:
    virtual ~VideoDecoderListener() = default;
    virtual void onDecodedFrame(gecko::camera::YCbCrFrame frame) = 0;
    virtual void onDecoderError(std::string errorDescription) = 0;
    virtual void onDecoderEOS() = 0;
};

struct VideoDecoderMetadata {
    CodecType codecType;
    int width;
    int height;
    int framerate;
    void *codecSpecific;
    size_t codecSpecificSize;
};

class VideoDecoder
{
public:
    virtual ~VideoDecoder() = default;
    virtual bool init(VideoDecoderMetadata metadata) = 0;
    // May block if the codec queue is full.
    virtual bool decode(const uint8_t *data,
                        size_t size,
                        uint64_t timestampUs,
                        FrameType frameType,
                        void (*releaseCallback)(void *),
                        void *releaseCallbackData) = 0;
    virtual void flush() = 0;
    virtual void drain() = 0;
    virtual void stop() = 0;

    void setListener(VideoDecoderListener *listener)
    {
        m_decoderListener = listener;
    }

protected:
    VideoDecoderListener *m_decoderListener = nullptr;
};

class CodecManager
{
public:
    virtual ~CodecManager() = default;
    virtual bool init() = 0;

    // Check if the codec is supported. May return false if all suitable
    // codecs are busy.
    virtual bool videoEncoderAvailable(CodecType codecType) = 0;
    virtual bool videoDecoderAvailable(CodecType codecType) = 0;

    // May return false if all suitable codecs are busy.
    virtual bool createVideoEncoder(CodecType codecType,
                                    std::shared_ptr<VideoEncoder> &encoder) = 0;
    virtual bool createVideoDecoder(CodecType codecType,
                                    std::shared_ptr<VideoDecoder> &decoder) = 0;
};

} // namespace codec
} // namespace gecko

extern "C" __attribute__((visibility("default")))
gecko::codec::CodecManager *gecko_codec_manager(void);

#endif // __GECKOCAMERA_CODEC__
/* vim: set ts=4 et sw=4 tw=80: */
