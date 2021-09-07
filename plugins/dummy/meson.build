dummy_plugin_source = [
  'dummy-camera.cpp',
]

dummy_plugin = shared_library('geckocamera-dummy',
		       dummy_plugin_source,
		       install: true,
                       include_directories: root_dir,
		       dependencies: dependency('threads'),
		       install_dir: plugins_install_dir )

plugins = [dummy_plugin]
