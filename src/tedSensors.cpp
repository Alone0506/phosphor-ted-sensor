#include "tedSensors.hpp"

#include "tedSensor.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <fstream>

static constexpr auto sensorDbusPath = "/xyz/openbmc_project/sensors/";

PHOSPHOR_LOG2_USING_WITH_FLAGS;

namespace phosphor::ted_sensor
{
std::map<std::string, ValueIface::Unit> unitMap = {
    {"temperature", ValueIface::Unit::DegreesC},
    {"fan_tach", ValueIface::Unit::RPMS},
    {"fan_pwm", ValueIface::Unit::Percent},
    {"voltage", ValueIface::Unit::Volts},
    {"altitude", ValueIface::Unit::Meters},
    {"current", ValueIface::Unit::Amperes},
    {"power", ValueIface::Unit::Watts},
    {"energy", ValueIface::Unit::Joules},
    {"utilization", ValueIface::Unit::Percent},
    {"airflow", ValueIface::Unit::CFM},
    {"pressure", ValueIface::Unit::Pascals}};

Json TedSensors::parseConfigFile()
{
    using path = std::filesystem::path;
    auto configFile = []() -> path {
        static constexpr auto name = "ted_sensor_config.json";

        for (auto path : {std::filesystem::current_path(),
                          path{"/var/lib/phosphor-ted-sensor"},
                          path{"/usr/share/phosphor-ted-sensor"}})
        {
            auto file = path / name;
            if (std::filesystem::exists(file))
            {
                return file;
            }
        }
        return name;
    }();

    std::ifstream jsonFile(configFile);
    if (!jsonFile.is_open())
    {
        lg2::error("config JSON file {FILENAME} not found", "FILENAME",
                   configFile);
        return {};
    }

    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        lg2::error("config readings JSON parser failure with {FILENAME}",
                   "FILENAME", configFile);
        throw std::exception{};
    }

    return data;
}

void TedSensors::createSensors()
{
    static const Json empty{};

    auto data = parseConfigFile();

    // print values
    lg2::debug("JSON: {JSON}", "JSON", data.dump());

    /* get ted sensor config data */
    for (const auto& j : data)
    {
        auto desc = j.value("Desc", empty);
        if (!desc.empty())
        {
            std::string name = desc.value("Name", "");
            std::replace(name.begin(), name.end(), ' ', '_');
            std::string sensorType = desc.value("SensorType", "");

            if (!name.empty() && !sensorType.empty())
            {
                if (unitMap.find(sensorType) == unitMap.end())
                {
                    lg2::error("Sensor type {TYPE} is not supported", "TYPE",
                               sensorType);
                }
                else
                {
                    if (tedSensorsMap.find(name) != tedSensorsMap.end())
                    {
                        lg2::error("A ted sensor named {NAME} already exists",
                                   "NAME", name);
                        continue;
                    }
                    auto objPath = sensorDbusPath + sensorType + "/" + name;

                    auto tedSensorPtr = std::make_unique<TedSensor>(
                        bus, objPath.c_str(), j, name, unitMap[sensorType]);

                    lg2::info("Added a new ted sensor: {NAME}", "NAME", name);

                    // During construction, because action::defer_emit is used,
                    // the "added" signal is not sent immediately. The D-Bus
                    // object remains in an "not yet emitted" state internally.
                    // Therefore, emit_object_added() must be called manually to
                    // emit the D-Bus object and trigger the object-added signal
                    // at once.
                    //
                    // During destruction, the sd_bus_emit_object_removed(path)
                    // signal will be sent to D-Bus.
                    tedSensorPtr->emit_object_added();

                    tedSensorsMap.emplace(name, std::move(tedSensorPtr));
                }
            }
            else
            {
                lg2::error(
                    "Sensor type ({TYPE}) or name ({NAME}) not found in config file",
                    "TYPE", sensorType, "NAME", name);
            }
        }
        else
        {
            lg2::error(
                "Descriptor for new ted sensor not found in config file");
        }
    }
}

void TedSensors::run()
{
    try
    {
        uint64_t interval = 1000000; // 1 sec
        _timer.restart(std::chrono::microseconds(interval));
    }
    catch (const std::exception& e)
    {
        lg2::error("Error in polling loop. {ERROR}", "ERROR", e.what());
    }
}
// systemctl status phosphor-ted-sensor.service
// busctl tree xyz.openbmc_project.TedSensor
// busctl introspect xyz.openbmc_project.TedSensor
// /xyz/openbmc_project/sensors/temperature/TestSensor
// mkdir -p /tmp/sensor/simulation
// echo 75 > /tmp/sensor/simulation/TestSensor
void TedSensors::read()
{
    static int c = 0;
    c++;

    for (auto& [name, sensor] : tedSensorsMap)
    {
        if (c == 40)
        {
            sensor->WarningIface::warningHigh(66);
        }
        sensor->read();
        sensor->checkThreshold();
    }
}
} // namespace phosphor::ted_sensor
