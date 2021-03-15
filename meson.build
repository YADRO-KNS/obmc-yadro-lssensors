project('list-sensors', 'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17',
    ],
    license: 'Apache-2.0',
    version: '2.0',
)

conf = configuration_data()

conf.set_quoted('MAPPER_SERVICE', get_option('mapper-service'))
conf.set_quoted('MAPPER_PATH', get_option('mapper-path'))
conf.set_quoted('MAPPER_IFACE', get_option('mapper-iface'))
conf.set_quoted('SENSORS_PATH', get_option('sensors-path'))
conf.set_quoted('SENSOR_VALUE_IFACE', get_option('sensor-value-iface'))

conf.set('WITH_REMOTE_HOST', get_option('remote-host-support'))

configure_file(output: 'config.h', configuration: conf)

executable('lssensors',
    'list-sensors.cpp',
    dependencies: [
        dependency('sdbusplus'),
    ],
    install: true,
    install_dir: get_option('sbindir'),
)
