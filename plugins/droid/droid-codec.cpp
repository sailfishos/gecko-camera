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

#include <cstring>
#include <sstream>
#include <functional>

#include <geckocamera-codec.h>
#include <droidmediacodec.h>
#include <droidmediaconstants.h>

#define LOG_TOPIC "droid-codec"
#include <geckocamera-utils.h>

#include "droid-common.h"

namespace gecko {
namespace codec {

using namespace std;
using namespace gecko::camera;

static const char *codecTypeToDroidMime(CodecType codecType)
{
    switch (codecType) {
    case VideoCodecVP8:
        return "video/x-vnd.on2.vp8";
    case VideoCodecVP9:
        return "video/x-vnd.on2.vp9";
    case VideoCodecH264:
        return "video/avc";
    case VideoCodecUnknown:
        break;
    }
    return NULL;
}

class DroidVideoFrameYUVMapper
{
public:
    DroidVideoFrameYUVMapper() : m_ready(false)
    {
    }

    bool setFormat(DroidMediaCodecMetaData *md, DroidMediaRect *rect)
    {
        DroidMediaColourFormatConstants c;
        droid_media_colour_format_constants_init(&c);

        m_template.y = 0;
        m_template.width = md->width;
        m_template.height = rect->bottom - rect->top;
#define _ALIGN_SIZE(sz, align) (((sz) + (align) - 1) & ~((align) - 1))
        if (md->hal_format == c.QOMX_COLOR_FormatYUV420PackedSemiPlanar32m) {
            unsigned int height = _ALIGN_SIZE(md->height, 32);
            m_template.yStride = m_template.cStride = _ALIGN_SIZE(md->width, 128);
            m_template.cb = (const uint8_t *)(m_template.yStride * height);
            m_template.cr = (const uint8_t *)(m_template.yStride * height + 1);
            m_template.chromaStep = 2;
        } else if (md->hal_format == c.OMX_COLOR_FormatYUV420SemiPlanar) {
            uint32_t height = md->height;
            m_template.yStride = m_template.cStride = _ALIGN_SIZE(md->width, 16);
            m_template.cb = (const uint8_t *)(m_template.yStride * height);
            m_template.cr = (const uint8_t *)(m_template.yStride * height + 1);
            m_template.chromaStep = 2;
        } else if (md->hal_format == c.OMX_COLOR_FormatYUV420Planar) {
            uint32_t height = _ALIGN_SIZE(md->height, 4);
            m_template.yStride = md->width;
            m_template.cStride = md->width / 2;
            m_template.cb = (const uint8_t *)(m_template.yStride * height);
            m_template.cr = (const uint8_t *)(m_template.yStride * height
                                              + (m_template.yStride * height) / 4);
            m_template.chromaStep = 1;
        } else {
            LOGE("Unsupported color format " << md->hal_format);
            return false;
        }
#undef _ALIGN_SIZE
        m_ready = true;
        return true;
    }

    YCbCrFrame mapYCbCr(DroidMediaCodecData *decoded) const
    {
        YCbCrFrame frame = m_template;
        uint8_t *data = static_cast<uint8_t *>(decoded->data.data);
        frame.y  = data + (unsigned long)m_template.y;
        frame.cb = data + (unsigned long)m_template.cb;
        frame.cr = data + (unsigned long)m_template.cr;
        frame.timestampUs = decoded->ts / 1000;
        return frame;
    }

    bool ready() const
    {
        return m_ready;
    }

    void reset()
    {
        m_ready = false;
    }

private:
    YCbCrFrame m_template;
    bool m_ready;
};

class DroidCodecManager : public CodecManager
{
public:
    DroidCodecManager() {};
    ~DroidCodecManager() {};
    bool init();

    bool videoEncoderAvailable(CodecType codecType);
    bool videoDecoderAvailable(CodecType codecType);

    bool createVideoEncoder(CodecType codecType, shared_ptr<VideoEncoder> &encoder);
    bool createVideoDecoder(CodecType codecType, shared_ptr<VideoDecoder> &decoder);

    static bool optionUseMediaBuffers();
};

class DroidVideoEncoder : public VideoEncoder
{
public:
    static shared_ptr<DroidVideoEncoder> create(CodecType codecType)
    {
        return make_shared<DroidVideoEncoder>(codecType);
    }
    explicit DroidVideoEncoder(CodecType codecType);
    ~DroidVideoEncoder();

    bool init(VideoEncoderMetadata metadata);
    bool encode(shared_ptr<const YCbCrFrame> frame, bool forceSync);

    void dataAvailable(DroidMediaCodecData *encoded);
    void error(string errorDescription);

private:
    static void error_cb(void *data, int err);
    static void signal_eos_cb(void *data);
    static void DataAvailableCallback(void *data, DroidMediaCodecData *encoded);

    CodecType m_codecType;
    DroidMediaCodecEncoderMetaData m_metadata;
    DroidMediaCodec *m_codec = nullptr;
    DroidMediaColourFormatConstants m_constants;
};

class DroidVideoDecoder : public VideoDecoder, public DroidObject
{
public:
    static shared_ptr<DroidVideoDecoder> create(CodecType codecType)
    {
        return make_shared<DroidVideoDecoder>(codecType);
    }
    explicit DroidVideoDecoder(CodecType codecType);
    ~DroidVideoDecoder();

    bool init(VideoDecoderMetadata metadata) override;
    bool decode(const uint8_t *data,
                size_t size,
                uint64_t timestampUs,
                FrameType frameType,
                void (*release)(void *),
                void *releaseData) override;
    void drain() override;
    void flush() override;
    void stop() override;

    void dataAvailable(DroidMediaCodecData *decoded);
    void configureOutput();
    void error(string errorDescription);

    bool ProcessMediaBuffer(DroidMediaBuffer *droidBuffer);

private:
    bool createCodec();

    static void dummyRelease(void *);

    static void data_available_cb(void *data, DroidMediaCodecData *decoded);
    static void error_cb(void *data, int err);
    static int size_changed_cb(void *data, int32_t width, int32_t height);
    static void signal_eos_cb(void *data);
    static bool buffer_created(void *data, DroidMediaBuffer *buffer);
    static void buffers_released(void *data);
    static bool frame_available(void *data, DroidMediaBuffer *buffer);

    CodecType m_codecType;
    DroidMediaCodecDecoderMetaData m_metadata;
    DroidMediaCodec *m_codec = nullptr;
    DroidVideoFrameYUVMapper m_mapper;
    DroidMediaBufferQueue *m_buffer_queue = nullptr;
    bool m_use_media_buffers = false;
    DroidGraphicBufferPool m_bufferPool;
};

bool DroidCodecManager::init()
{
    return droid_media_init();
}

bool DroidCodecManager::videoEncoderAvailable(CodecType codecType)
{
    DroidMediaCodecMetaData metadata;

    memset(&metadata, 0x0, sizeof (metadata));
    metadata.flags = static_cast<DroidMediaCodecFlags>(DROID_MEDIA_CODEC_HW_ONLY);
    metadata.type = codecTypeToDroidMime(codecType);

    if (metadata.type
            && droid_media_codec_is_supported(&metadata, true)) {
        LOGD(codecType << " true");
        return true;
    }
    LOGD(codecType << " false");
    return false;
}

bool DroidCodecManager::videoDecoderAvailable(CodecType codecType)
{
    DroidMediaCodecMetaData metadata;

    memset(&metadata, 0x0, sizeof (metadata));
    if (optionUseMediaBuffers()) {
        metadata.flags = static_cast<DroidMediaCodecFlags>(DROID_MEDIA_CODEC_HW_ONLY);
    } else {
        metadata.flags = static_cast<DroidMediaCodecFlags>(DROID_MEDIA_CODEC_HW_ONLY |
                                                           DROID_MEDIA_CODEC_NO_MEDIA_BUFFER);
    }
    metadata.type = codecTypeToDroidMime(codecType);

    if (metadata.type
            && droid_media_codec_is_supported(&metadata, false)) {
        LOGD(codecType << " true");
        return true;
    }
    LOGD(codecType << " false");
    return false;
}

bool DroidCodecManager::createVideoEncoder(CodecType codecType, shared_ptr<VideoEncoder> &encoder)
{
    LOGD("");
    encoder = DroidVideoEncoder::create(codecType);
    return true;
}

bool DroidCodecManager::createVideoDecoder(CodecType codecType, shared_ptr<VideoDecoder> &decoder)
{
    LOGD("");
    decoder = DroidVideoDecoder::create(codecType);
    return true;
}

// static
bool DroidCodecManager::optionUseMediaBuffers()
{
    if (DroidSystemInfo::envIsSet("GECKO_CAMERA_DROID_NO_MEDIA_BUFFER")) {
        return false;
    }
    if (DroidSystemInfo::envIsSet("GECKO_CAMERA_DROID_FORCE_MEDIA_BUFFER")) {
        return true;
    }

    DroidMediaColourFormatConstants c;
    droid_media_colour_format_constants_init (&c);

    // droidmedia on Android < 5 reports OMX_COLOR_FormatYUV420Flexible as 0
    return c.OMX_COLOR_FormatYUV420Flexible != 0
        && DroidSystemInfo::get().cpuVendor == DroidSystemInfo::CpuVendor::MediaTek;
}

DroidVideoEncoder::DroidVideoEncoder(CodecType codecType)
    : m_codecType(codecType)
{
    LOGD("codecType " << codecType);
    memset(&m_metadata, 0, sizeof(m_metadata));
}

DroidVideoEncoder::~DroidVideoEncoder()
{
    if (m_codec) {
        LOGD("");

        droid_media_codec_stop(m_codec);
        droid_media_codec_destroy(m_codec);
    }
}

bool DroidVideoEncoder::init(VideoEncoderMetadata metadata)
{
    LOGV("Init encode");

    if (m_codec) {
        LOGE("Encoder already initialized");
        return false;
    }

    m_metadata.parent.flags =
        static_cast<DroidMediaCodecFlags>(DROID_MEDIA_CODEC_HW_ONLY);
    m_metadata.parent.type = codecTypeToDroidMime(m_codecType);

    // Check if this device supports the codec we want
    if (!m_metadata.parent.type
            || !droid_media_codec_is_supported(&m_metadata.parent, true)) {
        LOGE("Codec not supported: " << m_metadata.parent.type);
        return false;
    }

    if (m_codecType == VideoCodecH264) {
        // TODO: Some devices may not support this feature. A workaround is
        // to save AVCC data and put it before every IDR manually.
        m_metadata.codec_specific.h264.prepend_header_to_sync_frames = true;
    }

    m_metadata.parent.width = metadata.width;
    m_metadata.parent.height = metadata.height;
    m_metadata.parent.fps = metadata.framerate;
    m_metadata.bitrate = metadata.bitrate;
    m_metadata.stride = metadata.stride;
    m_metadata.slice_height = metadata.sliceHeight;
    m_metadata.meta_data = false;
    m_metadata.bitrate_mode = DROID_MEDIA_CODEC_BITRATE_CONTROL_CBR;

    droid_media_colour_format_constants_init (&m_constants);
    m_metadata.color_format = -1;

    {
        uint32_t supportedFormats[32];
        unsigned int nFormats = droid_media_codec_get_supported_color_formats(
                                    &m_metadata.parent, 1, supportedFormats, 32);

        LOGI("Found " << nFormats << " color formats supported:");
        for (unsigned int i = 0; i < nFormats; i++) {
            int fmt = static_cast<int>(supportedFormats[i]);
            LOGI("  " << std::hex << fmt << std::dec);
            // The list of formats is sorted in order of codec's preference,
            // so pick the first one supported.
            if (m_metadata.color_format == -1 &&
                    (fmt == m_constants.OMX_COLOR_FormatYUV420Planar ||
                     fmt == m_constants.OMX_COLOR_FormatYUV420SemiPlanar)) {
                m_metadata.color_format = fmt;
            }
        }
    }

    if (m_metadata.color_format == -1) {
        LOGE("No supported color format found");
        return false;
    }

    LOGI("InitEncode: Codec metadata prepared: " << m_metadata.parent.type
         << " width=" << m_metadata.parent.width
         << " height=" << m_metadata.parent.height
         << " fps=" << m_metadata.parent.fps
         << " bitrate=" << m_metadata.bitrate
         << " color_format=" << m_metadata.color_format);

    m_codec = droid_media_codec_create_encoder (&m_metadata);
    if (!m_codec) {
        LOGE("Failed to create the encoder");
        return false;
    }

    LOGI("Codec created for " << m_metadata.parent.type);
    {
        DroidMediaCodecCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.error = DroidVideoEncoder::error_cb;
        cb.signal_eos = DroidVideoEncoder::signal_eos_cb;
        droid_media_codec_set_callbacks(m_codec, &cb, this);
    }

    {
        DroidMediaCodecDataCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.data_available = DroidVideoEncoder::DataAvailableCallback;
        droid_media_codec_set_data_callbacks(m_codec, &cb, this);
    }

    LOGD("Starting the encoder..");
    int result = droid_media_codec_start(m_codec);
    if (result == 0) {
        droid_media_codec_stop(m_codec);
        droid_media_codec_destroy(m_codec);
        m_codec = nullptr;
        LOGE("Failed to start the encoder!");
        return false;
    }

    LOGD("Encoder started");
    return true;
}

bool DroidVideoEncoder::encode(shared_ptr<const YCbCrFrame> frame, bool forceSync)
{
    LOGV("Encode: timestamp=" << frame->timestampUs << " forceSync=" << forceSync);

    DroidMediaCodecData data;
    DroidMediaBufferCallbacks cb;

    if (!m_codec) {
        LOGE("Encoder is not initialized");
        return false;
    }

    // Copy the frame to contiguous memory buffer, assume it's I420 frame from Gecko.
    // TODO: Check if the input frame is already I420 and contiguous memory.
    //       Handle other formats as well.
    const unsigned y_size = frame->yStride * frame->height;
    const unsigned u_size = y_size / 4;
    const unsigned v_size = y_size / 4;
    uint8_t *buf;

    LOGV("plane sizes: " << y_size
         << " " << u_size
         << " " << v_size
         << " timestamp: " << frame->timestampUs
         << " forceSync: " << forceSync);

    buf = (uint8_t *)malloc (y_size + u_size + v_size);
    data.data.data = buf;
    data.data.size = y_size + u_size + v_size;

    memcpy(buf, frame->y, y_size);
    buf += y_size;
    if (m_metadata.color_format == m_constants.OMX_COLOR_FormatYUV420Planar) {
        memcpy(buf, frame->cb, u_size);
        buf += u_size;
        memcpy(buf, frame->cr, v_size);
    } else {
        const uint8_t *inpU = static_cast<const uint8_t *>(frame->cb);
        const uint8_t *inpV = static_cast<const uint8_t *>(frame->cr);
        for (unsigned i = 0; i < u_size + v_size; i += 2) {
            buf[i] = *inpU++;
            buf[i + 1] = *inpV++;
        }
    }

    data.ts = frame->timestampUs;
    data.sync = forceSync;

    cb.unref = free;
    cb.data = data.data.data;

    droid_media_codec_queue (m_codec, &data, &cb);

    return true;
}

#if 0
static void dump(uint8_t *p, size_t size)
{
    unsigned int i;

    for (i = 0; i < size; i++ ) {
        if (i && (i % 16) == 0) {
            printf("\n");
        }
        printf(" %.2x", *p++);
    }
    if (i % 16) {
        printf("\n");
    }
}
#else
#define dump(p, s) do {} while (0)
#endif

void DroidVideoEncoder::dataAvailable(DroidMediaCodecData *encoded)
{
    LOGV("encoded data at " << (const void *)encoded->data.data
         << " length " << encoded->data.size
         << " timestamp " << encoded->ts / 1000
         << (encoded->sync ? " sync" : ""));
    dump((uint8_t *)encoded->data.data, encoded->data.size);

    if (m_encoderListener) {
        FrameType ft = encoded->sync ? KeyFrame : DeltaFrame;
        m_encoderListener->onEncodedFrame((uint8_t *)encoded->data.data,
                                          encoded->data.size, encoded->ts / 1000, ft);
    }
}

void DroidVideoEncoder::error(string errorDescription)
{
    LOGE(errorDescription);

    if (m_encoderListener) {
        m_encoderListener->onEncoderError(errorDescription);
    }
}

void DroidVideoEncoder::error_cb(void *data, int err)
{
    DroidVideoEncoder *encoder = static_cast<DroidVideoEncoder *>(data);

    ostringstream errorDesc;
    errorDesc << "Hardware error " << err;
    encoder->error(errorDesc.str());
}

void DroidVideoEncoder::signal_eos_cb(void *data)
{
    LOGI("Encoder EOS");
}

void DroidVideoEncoder::DataAvailableCallback(void *data, DroidMediaCodecData *encoded)
{
    DroidVideoEncoder *encoder = static_cast<DroidVideoEncoder *>(data);
    encoder->dataAvailable(encoded);
}

DroidVideoDecoder::DroidVideoDecoder(CodecType codecType)
    : m_codecType(codecType)
{
    memset(&m_metadata, 0, sizeof(m_metadata));
}

DroidVideoDecoder::~DroidVideoDecoder()
{
    LOGD("");

    stop();

    if (m_metadata.codec_data.data) {
        free(m_metadata.codec_data.data);
    }
}

bool DroidVideoDecoder::init(VideoDecoderMetadata metadata)
{
    memset (&m_metadata, 0x0, sizeof (m_metadata));
    m_use_media_buffers = DroidCodecManager::optionUseMediaBuffers();

    if (m_use_media_buffers) {
        DroidMediaColourFormatConstants c;
        droid_media_colour_format_constants_init (&c);
        m_metadata.color_format = c.OMX_COLOR_FormatYUV420Flexible;
        m_metadata.parent.flags =
            static_cast <DroidMediaCodecFlags> (DROID_MEDIA_CODEC_HW_ONLY);
    } else {
        m_metadata.parent.flags =
            static_cast <DroidMediaCodecFlags> (DROID_MEDIA_CODEC_HW_ONLY |
                                                DROID_MEDIA_CODEC_NO_MEDIA_BUFFER);
    }
    m_metadata.parent.type = codecTypeToDroidMime(m_codecType);
    if (!m_metadata.parent.type) {
        LOGE("Unknown codec " << m_codecType);
        return false;
    }

    if (!droid_media_codec_is_supported(&m_metadata.parent, false)) {
        LOGE("Codec not supported: " << m_metadata.parent.type);
    }

    m_metadata.parent.width = metadata.width;
    m_metadata.parent.height = metadata.height;
    m_metadata.parent.fps = metadata.framerate;

    if (metadata.codecSpecific && metadata.codecSpecificSize
            && m_codecType == VideoCodecH264) {
        // Copy AVCC data
        m_metadata.codec_data.size = metadata.codecSpecificSize;
        m_metadata.codec_data.data = malloc(metadata.codecSpecificSize);
        if (!m_metadata.codec_data.data) {
            LOGE("Cannot allocate memory");
            return false;
        }
        memcpy(m_metadata.codec_data.data,
               metadata.codecSpecific, metadata.codecSpecificSize);
        LOGD("Got H264 codec data size: " << (metadata.codecSpecificSize));
    } else {
        m_metadata.codec_data.size = 0;
    }

    LOGI("Codec metadata:"
         << " type=" << m_metadata.parent.type
         << " width=" << m_metadata.parent.width
         << " height=" << m_metadata.parent.height
         << " fps=" << m_metadata.parent.fps
         << " extra=" << m_metadata.codec_data.size);
    return true;
}

bool DroidVideoDecoder::createCodec()
{
    m_codec = droid_media_codec_create_decoder (&m_metadata);
    if (!m_codec) {
        m_codec = nullptr;
        LOGE("Failed to create the decoder");
        return false;
    }

    {
        DroidMediaCodecCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.error = DroidVideoDecoder::error_cb;
        cb.size_changed = DroidVideoDecoder::size_changed_cb;
        cb.signal_eos = DroidVideoDecoder::signal_eos_cb;
        droid_media_codec_set_callbacks(m_codec, &cb, this);
    }

    m_buffer_queue = m_use_media_buffers ?
                    droid_media_codec_get_buffer_queue(m_codec) : nullptr;
    if (m_buffer_queue) {
        LOGI("Using media buffers");
        DroidMediaBufferQueueCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.buffers_released = DroidVideoDecoder::buffers_released;
        cb.buffer_created = DroidVideoDecoder::buffer_created;
        cb.frame_available = DroidVideoDecoder::frame_available;
        droid_media_buffer_queue_set_callbacks (m_buffer_queue, &cb, this);
    } else {
        LOGI("Not using media buffers");
        DroidMediaCodecDataCallbacks cb;
        memset(&cb, 0, sizeof(cb));
        cb.data_available = DroidVideoDecoder::data_available_cb;
        droid_media_codec_set_data_callbacks(m_codec, &cb, this);
        m_use_media_buffers = false;
    }

    if (!droid_media_codec_start (m_codec)) {
        droid_media_codec_destroy (m_codec);
        m_codec = nullptr;
        LOGE("Failed to start the decoder");
        return false;
    }

    configureOutput();

    LOGD("Decoder started for " << m_metadata.parent.type);

    return true;
}

bool DroidVideoDecoder::ProcessMediaBuffer(DroidMediaBuffer *droidBuffer)
{
    if (droidBuffer && m_decoderListener) {
        shared_ptr<GraphicBuffer> buffer = m_bufferPool.acquire(droidBuffer);
        if (buffer) {
            m_decoderListener->onDecodedGraphicBuffer(buffer);
            return true;
        } else {
            LOGE("Couldn't find the buffer in the buffer pool");
        }
    }

    return false;
}

void DroidVideoDecoder::buffers_released(void *data)
{
    DroidVideoDecoder *decoder = (DroidVideoDecoder *)data;
    decoder->m_bufferPool.clear();
}

bool DroidVideoDecoder::buffer_created(void *data, DroidMediaBuffer *buffer)
{
    DroidVideoDecoder *decoder = (DroidVideoDecoder *)data;
    return decoder->m_bufferPool.bind(decoder, buffer);
}

bool DroidVideoDecoder::frame_available(void *data, DroidMediaBuffer *buffer)
{
    DroidVideoDecoder *decoder = (DroidVideoDecoder *)data;
    return decoder->ProcessMediaBuffer(buffer);
}

// This is called when the user has not provided a release callback.
void DroidVideoDecoder::dummyRelease(void *)
{
}

bool DroidVideoDecoder::decode(const uint8_t *data,
                               size_t size,
                               uint64_t timestampUs,
                               FrameType frameType,
                               void (*release)(void *),
                               void *releaseData)
{
    DroidMediaBufferCallbacks cb;
    DroidMediaCodecData cdata;

    LOGV("Decode: timestamp=" << timestampUs << " frameType" << frameType);

    if (!m_codec && !createCodec()) {
        LOGE("Cannot create decoder");
        return false;
    }

    cdata.ts = timestampUs;
    cdata.sync = frameType == KeyFrame;
    cdata.data.size = size;
    cdata.data.data = (void *)(data);

    cb.data = releaseData;
    cb.unref = release ? release : DroidVideoDecoder::dummyRelease;

    // This blocks when the input queue is full
    droid_media_codec_queue (m_codec, &cdata, &cb);

    LOGV("Frame queued to decoder");

    return true;
}

void DroidVideoDecoder::drain()
{
    LOGD("");
    if (m_codec) {
        droid_media_codec_drain(m_codec);
    }
}

void DroidVideoDecoder::flush()
{
    LOGD("");
    if (m_codec) {
        droid_media_codec_flush(m_codec);
        stop();
    }
}

void DroidVideoDecoder::stop()
{
    LOGD("");
    if (m_codec) {
        LOGD("");
        droid_media_codec_drain(m_codec);
        m_mapper.reset();
        droid_media_codec_stop(m_codec);
        droid_media_codec_destroy(m_codec);
        m_buffer_queue = nullptr;
        m_codec = nullptr;
    }
}

void DroidVideoDecoder::configureOutput()
{
    DroidMediaCodecMetaData md;
    DroidMediaRect rect;

    memset (&md, 0x0, sizeof (md));
    memset (&rect, 0x0, sizeof (rect));

    droid_media_codec_get_output_info (m_codec, &md, &rect);
    LOGI("Configuring converter for"
         << " stride:" << md.width
         << " slice-height: " << md.height
         << " top: " << rect.top << " left:" << rect.left
         << " width: " << rect.right - rect.left
         << " height: " << rect.bottom - rect.top
         << " format: " << md.hal_format);
    if (!m_use_media_buffers) {
        m_mapper.setFormat(&md, &rect);
    }
}

void DroidVideoDecoder::error(string errorDescription)
{
    LOGE(errorDescription);

    if (m_decoderListener) {
        m_decoderListener->onDecoderError(errorDescription);
    }
}

void DroidVideoDecoder::dataAvailable(DroidMediaCodecData *decoded)
{
    if (m_decoderListener && m_mapper.ready()) {
        const YCbCrFrame frame = m_mapper.mapYCbCr(decoded);
        m_decoderListener->onDecodedYCbCrFrame(&frame);
    }
}

void DroidVideoDecoder::data_available_cb(void *data, DroidMediaCodecData *decoded)
{
    DroidVideoDecoder *decoder = static_cast<DroidVideoDecoder *>(data);
    decoder->dataAvailable(decoded);
}

int DroidVideoDecoder::size_changed_cb(void *data, int32_t width, int32_t height)
{
    DroidVideoDecoder *decoder = static_cast<DroidVideoDecoder *>(data);
    LOGI("Received size changed " << width << " x " << height);
    decoder->configureOutput();
    return 0;
}

void DroidVideoDecoder::signal_eos_cb(void *data)
{
    LOGI("Decoder EOS");
}

void DroidVideoDecoder::error_cb(void *data, int err)
{
    DroidVideoDecoder *decoder = static_cast<DroidVideoDecoder *>(data);

    ostringstream errorDesc;
    errorDesc << "Hardware error " << err;
    decoder->error(errorDesc.str());
}

static DroidCodecManager codecManagerDroid;

} // namespace codec
} // namespace gecko

extern "C" __attribute__((visibility("default")))
gecko::codec::CodecManager *gecko_codec_plugin_manager(void)
{
    gecko::codec::codecManagerDroid.init();
    return &gecko::codec::codecManagerDroid;
}
/* vim: set ts=4 et sw=4 tw=80: */
