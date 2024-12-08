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

#include <iostream>
#include <memory>
#include <cstring>
#include <thread>
#include <getopt.h>

#include "geckocamera.h"
#include "geckocamera-codec.h"

using namespace std;
using namespace gecko::camera;
using namespace gecko::codec;

class GeckoCameraExample
    : CameraListener
    , VideoEncoderListener
    , VideoDecoderListener
{
public:
    GeckoCameraExample()
        : cameraManager(gecko_camera_manager())
        , codecManager(gecko_codec_manager())
        , encoderAvailable(false)
        , decoderAvailable(false)
        , frameNumber(0)
    {
    }

    int run(unsigned int cameraNumber, unsigned int modeNumber, unsigned int durationSeconds)
    {
        vector<CameraInfo> cameraList;

        for (int i = 0; i < cameraManager->getNumberOfCameras(); i++) {
            CameraInfo info;
            if (cameraManager->getCameraInfo(i, info)) {
                cameraList.push_back(info);
                cout << "Found camera " << i << "\n"
                     << "    id         :" << info.id << "\n"
                     << "    name       :" << info.name << "\n"
                     << "    provider   :" << info.provider << "\n"
                     << "    facing     :" << (info.facing == GECKO_CAMERA_FACING_FRONT ?
                                               "front" : "rear") << "\n"
                     << "    mountAngle :" << info.mountAngle << "\n";
            }
        }

        cout << cameraList.size() << " cameras found\n";

        if (cameraNumber < cameraList.size()) {
            const CameraInfo info = cameraList.at(cameraNumber);
            vector<CameraCapability> caps;
            if (cameraManager->queryCapabilities(info.id, caps) && modeNumber < caps.size()) {
                cout << "Camera " << info.id << " caps:\n";
                for (const CameraCapability &cap : caps) {
                    cout << "    " << cap.width << "x" << cap.height << ":" << cap.fps << "\n";
                }

                shared_ptr<Camera> camera;
                if (cameraManager->openCamera(info.id, camera)) {
                    const CameraCapability cap = caps.at(modeNumber);
                    camera->setListener(this);

                    encoderAvailable = initEncoder(cap);
                    cout << "Video encoder " << (encoderAvailable ? "available" : "not available") << "\n";

                    decoderAvailable = initDecoder(cap);
                    cout << "Video decoder " << (decoderAvailable ? "available" : "not available") << "\n";

                    if (camera->startCapture(cap)) {
                        this_thread::sleep_for(chrono::seconds(durationSeconds));
                        camera->stopCapture();
                        return 0;
                    } else {
                        cerr << "Cannot start capture\n";
                    }
                }
            } else {
                cerr << "Camera has no mode " << modeNumber << "\n";
            }
        } else {
            cerr << "Camera numer " << cameraNumber << " not found\n";
        }
        return -1;
    }

private:
    class EncodedFrame
    {
    public:
        EncodedFrame(uint8_t *encoded, size_t size, uint64_t timestampUs, FrameType type)
            : size(size)
            , timestampUs(timestampUs)
            , type(type)
        {
            data = static_cast<uint8_t *>(malloc(size));
            memcpy(data, encoded, size);
            cout << "Create encoded frame " << this << "\n";
        }

        ~EncodedFrame()
        {
            free(data);
        }

        static void release(void *data)
        {
            EncodedFrame *frame = static_cast<EncodedFrame *>(data);
            cout << "Release encoded frame " << frame << "\n";
            delete frame;
        }

        uint8_t *data;
        size_t size;
        uint64_t timestampUs;
        FrameType type;
    };

    bool initEncoder(CameraCapability cap)
    {
        if (codecManager->videoEncoderAvailable(VideoCodecH264) &&
                codecManager->createVideoEncoder(VideoCodecH264, videoEncoder)) {
            VideoEncoderMetadata meta;

            meta.codecType = VideoCodecH264;
            meta.width = cap.width;
            meta.height = cap.height;
            meta.stride = cap.width;
            meta.sliceHeight = cap.height;
            meta.bitrate = 2000000;
            meta.framerate = 30;

            cout << "Initializing encoder"
                 << " size " << meta.width << "x" << meta.height
                 << " bitrate " << meta.bitrate
                 << " framerate " << meta.framerate << "\n";

            if (videoEncoder->init(meta)) {
                cout << "  success!\n";
                videoEncoder->setListener(this);
                return true;
            }
        }
        return false;
    }

    bool initDecoder(CameraCapability cap)
    {
        if (codecManager->videoDecoderAvailable(VideoCodecH264) &&
                codecManager->createVideoDecoder(VideoCodecH264, videoDecoder)) {
            VideoDecoderMetadata meta;

            meta.codecType = VideoCodecH264;
            meta.width = cap.width;
            meta.height = cap.height;
            meta.framerate = 30;
            meta.codecSpecific = nullptr;
            meta.codecSpecificSize = 0;

            cout << "Initializing decoder"
                 << " size " << meta.width << "x" << meta.height
                 << " framerate " << meta.framerate << "\n";

            if (videoDecoder->init(meta)) {
                cout << "  success!\n";
                videoDecoder->setListener(this);
                return true;
            }
        }
        return false;
    }

    // Camera
    void onCameraFrame(shared_ptr<GraphicBuffer> buffer)
    {
        shared_ptr<const YCbCrFrame> frame = buffer->mapYCbCr();
        if (frame) {
            cout << "buffer at " << (const void *)frame->y
                 << " timestampUs " << frame->timestampUs
                 << "\n";

            if (encoderAvailable) {
                bool sync = !(frameNumber++ % 30);
                videoEncoder->encode(frame, sync);
            }
        }
    }

    void onCameraError(string errorDescription)
    {
        cout << "Camera error: " << errorDescription << "\n";
    }

    // Encoder
    void onEncodedFrame(uint8_t *data, size_t size, uint64_t timestampUs, FrameType type)
    {
        cout << "Encoded frame size " << size
             << " timestampUs " << timestampUs
             << (type == KeyFrame ? " sync" : "")
             << "\n";

        if (decoderAvailable) {
            EncodedFrame *frame = new EncodedFrame(data, size, timestampUs, type);
            // May block if the input queue is full. Not handled here for simplicity.
            videoDecoder->decode(frame->data, frame->size, frame->timestampUs,
                                 frame->type, &EncodedFrame::release, frame);
        }
    }

    void onEncoderError(string errorDescription)
    {
        cout << "Video encoder error: " << errorDescription << "\n";
    }

    // Decoder
    void onDecodedYCbCrFrame(const gecko::camera::YCbCrFrame *frame)
    {
        cout << "*Decoded buffer at " << (const void *)frame->y
             << " cb=" << (const void *)frame->cb
             << " cr=" << (const void *)frame->cr
             << " yStride=" << frame->yStride
             << " cStride=" << frame->cStride
             << " chromaStep=" << frame->chromaStep
             << " timestampUs " << frame->timestampUs
             << "\n";
    }

    void onDecodedGraphicBuffer(std::shared_ptr<gecko::camera::GraphicBuffer> buffer)
    {
        cout << "GraphicBuffer received" << "\n";
        shared_ptr<const YCbCrFrame> frame = buffer->mapYCbCr();
        if (frame) {
            onDecodedYCbCrFrame(frame.get());
        } else {
            cerr << "Couldn't map GraphicBuffer\n";
        }
    }

    void onDecoderError(std::string errorDescription)
    {
        cout << "Video decoder error: " << errorDescription << "\n";
    }

    void onDecoderEOS()
    {
        cout << "Video decoder EOS\n";
    }

    CameraManager *cameraManager = nullptr;
    CodecManager *codecManager = nullptr;
    shared_ptr<VideoEncoder> videoEncoder;
    bool encoderAvailable;
    shared_ptr<VideoDecoder> videoDecoder;
    bool decoderAvailable;
    unsigned int frameNumber;
};

int main(int argc, char *argv[])
{
    int opt;
    unsigned int cameraNumber = 0;
    // Do not use maximum resolution
    unsigned int modeNumber = 1;
    unsigned int durationSeconds = 10;

    while ((opt = getopt(argc, argv, "c:m:t:")) != -1) {
        switch (opt) {
        case 'c':
            cameraNumber = atoi(optarg);
            break;
        case 'm':
            modeNumber = atoi(optarg);
            break;
        case 't':
            durationSeconds = atoi(optarg);
            break;
        default:
            break;
        }
    }

    GeckoCameraExample app;
    return app.run(cameraNumber, modeNumber, durationSeconds);
}

/* vim: set ts=4 et sw=4 tw=80: */
