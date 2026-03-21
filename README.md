# phosphor-ted-sensor

phosphor-ted-sensor is a simulation sensor service for OpenBMC. It creates D-Bus
sensor objects based on a JSON configuration file and reads sensor values from
simulation files on disk, making it useful for development and testing without
real hardware.

## D-Bus

- **Service name:** `xyz.openbmc_project.TedSensor`
- **Object path:** `/xyz/openbmc_project/sensors/<SensorType>/<Name>`

Each sensor object implements the following D-Bus interfaces:

| Interface                                          | Description                             |
| -------------------------------------------------- | --------------------------------------- |
| `xyz.openbmc_project.Sensor.Value`                 | Sensor value, unit, min/max             |
| `xyz.openbmc_project.Sensor.Threshold.Warning`     | Warning high/low thresholds and alarms  |
| `xyz.openbmc_project.Sensor.Threshold.Critical`    | Critical high/low thresholds and alarms |
| `xyz.openbmc_project.State.Decorator.Availability` | Sensor availability                     |
| `xyz.openbmc_project.Association.Definitions`      | Inventory associations                  |

## Configuration

The service reads `ted_sensor_config.json` from the first location found:

1. The current working directory
2. `/var/lib/phosphor-ted-sensor`
3. `/usr/share/phosphor-ted-sensor`

By default the build installs a sample config into (3).

### Configuration format

The config file is a JSON array. Each element defines one sensor:

```json
[
  {
    "Desc": {
      "Name": "TedSensor1",
      "SensorType": "temperature",
      "MaxValue": 127.0,
      "MinValue": -128.0
    },
    "Threshold": {
      "CriticalHigh": 80,
      "WarningHigh": 60,
      "WarningLow": 10,
      "CriticalLow": 0
    },
    "Associations": [
      [
        "chassis",
        "all_sensors",
        "/xyz/openbmc_project/inventory/system/board/Ted_Board"
      ]
    ]
  }
]
```

| Field                    | Required | Description                                             |
| ------------------------ | -------- | ------------------------------------------------------- |
| `Desc.Name`              | Yes      | Sensor name (spaces are replaced with `_`)              |
| `Desc.SensorType`        | Yes      | Sensor type, determines the unit (see table below)      |
| `Desc.MaxValue`          | No       | Maximum sensor value for clamping                       |
| `Desc.MinValue`          | No       | Minimum sensor value for clamping                       |
| `Threshold.CriticalHigh` | No       | Critical high threshold                                 |
| `Threshold.CriticalLow`  | No       | Critical low threshold                                  |
| `Threshold.WarningHigh`  | No       | Warning high threshold                                  |
| `Threshold.WarningLow`   | No       | Warning low threshold                                   |
| `Associations`           | No       | D-Bus association tuples `[forward, reverse, endpoint]` |

### Supported sensor types

| SensorType    | Unit     |
| ------------- | -------- |
| `temperature` | DegreesC |
| `fan_tach`    | RPMS     |
| `fan_pwm`     | Percent  |
| `voltage`     | Volts    |
| `current`     | Amperes  |
| `power`       | Watts    |
| `energy`      | Joules   |
| `utilization` | Percent  |
| `altitude`    | Meters   |
| `airflow`     | CFM      |
| `pressure`    | Pascals  |

## How it works

1. On startup the service parses the JSON config and creates D-Bus sensor
   objects.
2. A periodic timer fires every **1 second** and reads each sensor's simulation
   file from `/tmp/sensor/simulation/<Name>`. The file should contain a single
   numeric value.
3. The value is clamped to `[MinValue, MaxValue]` and published on D-Bus.
4. After each read the service checks all configured thresholds and
   asserts/deasserts alarm signals as appropriate.

## Quick start (on BMC or QEMU)

```bash
# Check service status
systemctl status phosphor-ted-sensor.service

# Browse the D-Bus tree
busctl tree xyz.openbmc_project.TedSensor

# Inspect a sensor
busctl introspect xyz.openbmc_project.TedSensor \
    /xyz/openbmc_project/sensors/temperature/TedSensor1

# Write a simulated value
mkdir -p /tmp/sensor/simulation
echo 75 > /tmp/sensor/simulation/TedSensor1

# Remove & Add sensor (TedSensor2 as example)
busctl call xyz.openbmc_project.TedSensor /xyz/openbmc_project/AddRemoveSensor xyz.openbmc_project.AddRemoveSensor RemoveSensor s "TedSensor2"
busctl call xyz.openbmc_project.TedSensor /xyz/openbmc_project/AddRemoveSensor xyz.openbmc_project.AddRemoveSensor AddSensor s "TedSensor2"
```

## Note:

```bash
systemctl status phosphor-ted-sensor.service
busctl tree xyz.openbmc_project.TedSensor
busctl introspect xyz.openbmc_project.TedSensor /xyz/openbmc_project/sensors/temperature/TedSensor1
mkdir -p /tmp/sensor/simulation
echo 75 > /tmp/sensor/simulation/TedSensor1
dbus-monitor --system "type='signal',sender='xyz.openbmc_project.TedSensor'"
```

每秒看value:

```bash
watch -n1 "busctl get-property xyz.openbmc_project.TedSensor /xyz/openbmc_project/sensors/temperature/TedSensor1 xyz.openbmc_project.Sensor.Value Value"
```

dbus-monitor message (set TedSensor1 value to 75, which is >= WarningHigh
threshold):

```bash
root@evb-ast2600:~# dbus-monitor --system "type='signal',sender='xyz.openbmc_project.TedSensor'"
signal time=1743712513.484692 sender=org.freedesktop.DBus -> destination=:1.69 serial=4294967295 path=/org/freedesktop/DBus; interface=org.freedesktop.DBus; member=NameAcquired
   string ":1.69"
signal time=1743712513.486389 sender=org.freedesktop.DBus -> destination=:1.69 serial=4294967295 path=/org/freedesktop/DBus; interface=org.freedesktop.DBus; member=NameLost
   string ":1.69"
signal time=1773105290.385868 sender=:1.10 -> destination=(null destination) serial=50 path=/xyz/openbmc_project/sensors/temperature/TedSensor1; interface=org.freedesktop.DBus.Properties; member=PropertiesChanged
   string "xyz.openbmc_project.Sensor.Value"
   array [
      dict entry(
         string "Value"
         variant             double 75
      )
   ]
   array [
   ]
signal time=1773105290.386729 sender=:1.10 -> destination=(null destination) serial=51 path=/xyz/openbmc_project/sensors/temperature/TedSensor1; interface=org.freedesktop.DBus.Properties; member=PropertiesChanged
   string "xyz.openbmc_project.Sensor.Threshold.Warning"
   array [
      dict entry(
         string "WarningAlarmHigh"
         variant             boolean true
      )
   ]
   array [
   ]
signal time=1773105290.387094 sender=:1.10 -> destination=(null destination) serial=52 path=/xyz/openbmc_project/sensors/temperature/TedSensor1; interface=xyz.openbmc_project.Sensor.Threshold.Warning; member=WarningHighAlarmAsserted
   double 75
```

watch message:

```bash
Every 1.0s: busctl get-property xyz.openbmc_project.TedSensor /xyz/openbmc_project/sensors/temperature/TedSensor1 xyz.openbmc_project.Sensor.Value Value             2026-03-10 01:58:04

d 75
```

call method

- RemoveSensor:

```bash
root@evb-ast2600:~# busctl tree xyz.openbmc_project.TedSensor
`- /xyz
  `- /xyz/openbmc_project
    |- /xyz/openbmc_project/AddRemoveSensor
    `- /xyz/openbmc_project/sensors
      `- /xyz/openbmc_project/sensors/temperature
        |- /xyz/openbmc_project/sensors/temperature/TedSensor1
        `- /xyz/openbmc_project/sensors/temperature/TedSensor2

root@evb-ast2600:~# busctl call xyz.openbmc_project.TedSensor /xyz/openbmc_project/AddRemoveSensor xyz.openbmc_project.AddRemoveSensor RemoveSensor s "TedSensor2"
s "Removed ted sensor: TedSensor2"

root@evb-ast2600:~# busctl tree xyz.openbmc_project.TedSensor
`- /xyz
  `- /xyz/openbmc_project
    |- /xyz/openbmc_project/AddRemoveSensor
    `- /xyz/openbmc_project/sensors
      `- /xyz/openbmc_project/sensors/temperature
        `- /xyz/openbmc_project/sensors/temperature/TedSensor1
```

- AddSensor

```bash
root@evb-ast2600:~# busctl tree xyz.openbmc_project.TedSensor
`- /xyz
  `- /xyz/openbmc_project
    |- /xyz/openbmc_project/AddRemoveSensor
    `- /xyz/openbmc_project/sensors
      `- /xyz/openbmc_project/sensors/temperature
        `- /xyz/openbmc_project/sensors/temperature/TedSensor1

root@evb-ast2600:~# busctl call xyz.openbmc_project.TedSensor /xyz/openbmc_project/AddRemoveSensor xyz.openbmc_project.AddRemoveSensor AddSensor s "TedSensor2"
s "Added ted sensor: TedSensor2"

root@evb-ast2600:~# busctl tree xyz.openbmc_project.TedSensor
`- /xyz
  `- /xyz/openbmc_project
    |- /xyz/openbmc_project/AddRemoveSensor
    `- /xyz/openbmc_project/sensors
      `- /xyz/openbmc_project/sensors/temperature
        |- /xyz/openbmc_project/sensors/temperature/TedSensor1
        `- /xyz/openbmc_project/sensors/temperature/TedSensor2
```
