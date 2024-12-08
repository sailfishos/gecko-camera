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

#include <dlfcn.h>
#include <map>
#include <filesystem>
#include <mutex>

#include "geckocamera-codec.h"

#define LOG_TOPIC "codec"
#include "geckocamera-plugins.h"
#include "geckocamera-utils.h"

namespace gecko {
namespace codec {

using namespace std;
using namespace gecko::camera;

class RootCodecManager : public CodecManager
{
public:
    RootCodecManager() {};
    ~RootCodecManager() {};

    bool init() override;

    bool videoEncoderAvailable(CodecType codecType) override;
    bool videoDecoderAvailable(CodecType codecType) override;

    bool createVideoEncoder(CodecType codecType, shared_ptr<VideoEncoder> &encoder) override;
    bool createVideoDecoder(CodecType codecType, shared_ptr<VideoDecoder> &decoder) override;

private:
    shared_ptr<CodecManager> loadPlugin(Plugin &plugin);

    mutex m_mutex;
    bool m_initialized = false;
    map<const string, shared_ptr<CodecManager>> m_plugins;
    map<const string, shared_ptr<CodecManager>> m_mimeTypeMap;
};

bool RootCodecManager::init()
{
    scoped_lock lock(m_mutex);
    if (!m_initialized) {
        for (auto plugin : PluginManager::get()->listPlugins()) {
            auto manager = loadPlugin(plugin);
            if (manager && manager->init()) {
                LOGI("Initialized codec plugin at " << plugin.path);
                m_plugins.emplace(plugin.path, manager);
            }
        }
        m_initialized = true;
    }
    return true;
}

shared_ptr<CodecManager> RootCodecManager::loadPlugin(Plugin &plugin)
{
    if (plugin.handle) {
        CodecManager* (*_manager)() = (CodecManager * (*)())dlsym(plugin.handle, "gecko_codec_plugin_manager");
        if (_manager) {
            CodecManager *manager = _manager();
            if (manager) {
                // Create a shared_ptr that doesn't call delete() since manager is the pointer
                // to a static object.
                return shared_ptr<CodecManager>(shared_ptr<CodecManager> {}, manager);
            }
        }
    }
    return nullptr;
}

bool RootCodecManager::videoEncoderAvailable(CodecType codecType)
{
    scoped_lock lock(m_mutex);
    for (auto const& [path, plugin] : m_plugins) {
        if (plugin->videoEncoderAvailable(codecType))
            return true;
    }
    return false;
}

bool RootCodecManager::videoDecoderAvailable(CodecType codecType)
{
    scoped_lock lock(m_mutex);
    for (auto const& [path, plugin] : m_plugins) {
        if (plugin->videoDecoderAvailable(codecType))
            return true;
    }
    return false;
}

bool RootCodecManager::createVideoEncoder(CodecType codecType, shared_ptr<VideoEncoder> &encoder)
{
    scoped_lock lock(m_mutex);
    for (auto const& [path, plugin] : m_plugins) {
        if (plugin->createVideoEncoder(codecType, encoder))
            return true;
    }
    return false;
}

bool RootCodecManager::createVideoDecoder(CodecType codecType, shared_ptr<VideoDecoder> &decoder)
{
    scoped_lock lock(m_mutex);
    for (auto const& [path, plugin] : m_plugins) {
        if (plugin->createVideoDecoder(codecType, decoder))
            return true;
    }
    return false;
}

static RootCodecManager codecRootManager;

} // namespace codec
} // namespace gecko

extern "C" __attribute__((visibility("default")))
gecko::codec::CodecManager *gecko_codec_manager(void)
{
    gecko::codec::codecRootManager.init();
    return &gecko::codec::codecRootManager;
}

/* vim: set ts=4 et sw=4 tw=80: */
