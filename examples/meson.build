geckocamera_example_source = [
  'geckocamera_example.cpp',
]

geckocamera_example = executable('geckocamera-example',
    geckocamera_example_source,
    install: true,
    install_dir: get_option('datadir') / meson.project_name(),
    link_with: libgeckocamera_so,
    include_directories: root_dir)
