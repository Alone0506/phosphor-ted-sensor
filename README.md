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
      "Name": "TestSensor",
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
    /xyz/openbmc_project/sensors/temperature/TestSensor

# Write a simulated value
mkdir -p /tmp/sensor/simulation
echo 75 > /tmp/sensor/simulation/TestSensor
```

## Note:

```bash
systemctl status phosphor-ted-sensor.service
busctl tree xyz.openbmc_project.TedSensor
busctl introspect xyz.openbmc_project.TedSensor /xyz/openbmc_project/sensors/temperature/TestSensor
mkdir -p /tmp/sensor/simulation
echo 75 > /tmp/sensor/simulation/TestSensor
dbus-monitor --system "type='signal',sender='xyz.openbmc_project.TedSensor'"
```
