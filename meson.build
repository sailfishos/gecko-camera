project('gecko-camera', 'cpp',
  version : '0.1',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized',
                      'cpp_std=c++17'])

geckocamera_version = meson.project_version()
geckocamera_name = meson.project_name()

# Use libtool version numbers
libgeckocamera_lt_c=0
libgeckocamera_lt_r=0
libgeckocamera_lt_a=0

cc = meson.get_compiler('cpp')

root_dir = include_directories('.')

plugins_install_dir = '/'.join([ get_option('datadir'), meson.project_name(), 'plugins'])
add_project_arguments(['-DGECKO_CAMERA_PLUGIN_DIR="' +
                       '/'.join([get_option('prefix'), plugins_install_dir]) + '"'],
                       language: 'cpp')
add_project_link_arguments(['-lstdc++fs'], language : 'cpp')

if get_option('build-devel') == true or get_option('build-tests') == true
  geckocamera_source = [
    'geckocamera.cpp',
    'utils.cpp'
  ]

  libgeckocamera_so_version = '@0@.@1@.@2@'.format(
      (libgeckocamera_lt_c - libgeckocamera_lt_a),
      libgeckocamera_lt_a,
      libgeckocamera_lt_r)

  libgeckocamera_so = shared_library(
      'geckocamera',
      geckocamera_source,
      install: true,
      dependencies: cc.find_library('dl', required : false),
      version: libgeckocamera_so_version )

  geckocamera_headers = [
    'geckocamera.h',
    'geckocamera-utils.h',
  ]

  install_headers(geckocamera_headers, subdir : meson.project_name())

  import('pkgconfig').generate(
      libgeckocamera_so,
      libraries: ['-ldl'],
      subdirs: [meson.project_name()])
endif

if get_option('build-examples') == true
  subdir('examples')
endif

if get_option('build-plugins')
  subdir('plugins')
endif
