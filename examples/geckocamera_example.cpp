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
#include <thread>

#include <getopt.h>

#include "geckocamera.h"

using namespace std;
using namespace gecko::camera;

class GeckoCameraExample : CameraListener
{
public:
    GeckoCameraExample() : manager(gecko_camera_manager()) {}

    int run(unsigned int cameraNumber, unsigned int modeNumber, unsigned int durationSeconds)
    {
        vector<CameraInfo> cameraList;

        for (int i = 0; i < manager->getNumberOfCameras(); i++) {
            CameraInfo info;
            if (manager->getCameraInfo(i, info)) {
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
            CameraInfo info = cameraList.at(cameraNumber);
            vector<CameraCapability> caps;
            if (manager->queryCapabilities(info.id, caps) && modeNumber < caps.size()) {
                cout << "Camera " << info.id << " caps:\n";
                for (auto cap : caps) {
                    cout << "    " << cap.width << "x" << cap.height << ":" << cap.fps << "\n";
                }

                shared_ptr<Camera> camera;
                if (manager->openCamera(info.id, camera)) {
                    camera->setListener(this);
                    if (camera->startCapture(caps.at(modeNumber))) {
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
    void onCameraFrame(shared_ptr<const CameraFrame> frame)
    {
        cout << "buffer at " << frame->y << "\n";
    }
    void onCameraError(string errorDescription)
    {
        cout << "Error: " << errorDescription << "\n";
    }

    CameraManager *manager = nullptr;
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
