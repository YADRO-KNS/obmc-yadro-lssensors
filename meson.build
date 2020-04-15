project('list-sensors', 'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17',
    ],
    license: 'Apache-2.0',
    version: '2.0',
)

executable('lssensors',
    'list-sensors.cpp',
    dependencies: [
        dependency('sdbusplus'),
    ],
    install: true,
)
