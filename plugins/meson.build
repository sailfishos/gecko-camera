geckocamera_plugin_source = [
  'geckocamera_droid_plugin.cpp',
]

droidmedia_dep=dependency('droidmedia', required: false)

geckocamera_droid = shared_module('geckocamera-droid',
		       geckocamera_plugin_source,
		       install: true,
		       dependencies: droidmedia_dep,
                       include_directories: root_dir,
		       install_dir: plugins_install_dir )

plugins = [geckocamera_droid]
