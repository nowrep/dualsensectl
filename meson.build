project(
  'dualsensectl',
  'c',
  version: '0.7.0',
  license: 'GPLv2',
  meson_version: '>=0.58.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    ],
  )

add_project_arguments(
  [
    '-Wl,--exclude-libs=ALL',
    '-DDUALSENSECTL_VERSION="@0@"'.format(meson.project_version()),
    ],
  language: 'c',
  )

udev = dependency('libudev')
hidapi_hidraw = dependency('hidapi-hidraw')

executable(
  'dualsensectl',
  ['main.c'],
  dependencies: [udev, hidapi_hidraw],
  install: true,
  )
