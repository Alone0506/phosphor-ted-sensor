project(
    'phosphor-ted-sensor',
    'cpp',
    version: '1.0',
    default_options: [
        'buildtype=debugoptimized',
        'cpp_std=c++23',
        'warning_level=3',
        'werror=true',
    ],
    meson_version: '>=1.1.1',
)

pkgdatadir = get_option('datadir') / meson.project_name()

executable(
    'phosphor-ted-sensor',
    [
        'src/main.cpp',
        'src/tedSensor.cpp',
    ],
    dependencies: [
        dependency('nlohmann_json', include_type: 'system'),
        dependency('phosphor-dbus-interfaces'),
        dependency('phosphor-logging'),
        dependency('sdbusplus'),
        dependency('sdeventplus'),  # for timer
    ],
    install: true,
    install_dir: get_option('bindir'),
)

install_data('ted_sensor_config.json', install_dir: pkgdatadir)

systemd = dependency('systemd')
conf_data = configuration_data()
conf_data.set('bindir', get_option('prefix') / get_option('bindir'))
configure_file(
    input: 'phosphor-ted-sensor.service.in',
    output: 'phosphor-ted-sensor.service',
    configuration: conf_data,
    install: true,
    install_dir: systemd.get_variable('systemd_system_unit_dir'),
)
