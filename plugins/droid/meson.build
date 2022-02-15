droid_plugin_source = [
  'droid-camera.cpp',
  'droid-codec.cpp',
  'droid-common.cpp',
]

droidmedia_dep=dependency('droidmedia', required: false)

droid_plugin = shared_module('geckocamera-droid',
		       droid_plugin_source,
		       install: true,
		       dependencies: droidmedia_dep,
                       include_directories: root_dir,
		       install_dir: plugins_install_dir )

plugins = [droid_plugin]
