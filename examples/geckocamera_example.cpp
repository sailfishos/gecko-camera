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

#include "geckocamera.h"

using namespace std;
using namespace gecko::camera;

class GeckoCameraExample : CameraListener
{
public:
    GeckoCameraExample() : manager(gecko_camera_manager()) {}

    int run()
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

        if (cameraList.size()) {
            CameraInfo info = cameraList.at(0);
            vector<CameraCapability> caps;
            if (manager->queryCapabilities(info.id, caps)) {
                cout << "Camera " << info.id << " caps:\n";
                for (auto cap : caps) {
                    cout << "    " << cap.width << "x" << cap.height << ":" << cap.fps << "\n";
                }

                shared_ptr<Camera> camera;
                if (manager->openCamera(cameraList.at(0).id, camera)) {
                    camera->setListener(this);
                    if (camera->startCapture(caps.at(0))) {
                        this_thread::sleep_for(chrono::seconds(10));
                        camera->stopCapture();
                        return 0;
                    } else {
                        cout << "Cannot start capture\n";
                    }
                }
            } else {
                cout << "Camera 0 doesn't provide capabilities\n";
            }
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
    GeckoCameraExample app;
    return app.run();
}

/* vim: set ts=4 et sw=4 tw=80: */
