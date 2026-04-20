#include "tedSensors.hpp"

#include "tedSensor.hpp"

#include <boost/asio/error.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <chrono>
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

TedSensors::TedSensors(std::shared_ptr<sdbusplus::asio::connection>& conn) :
    conn(conn),
    objServer(sdbusplus::asio::object_server(conn, /*skipManager=*/true))
{
    objServer.add_manager("/xyz/openbmc_project/sensors");

    addRemoveSensorIface =
        objServer.add_interface("/xyz/openbmc_project/AddRemoveSensor",
                                "xyz.openbmc_project.AddRemoveSensor");
    registerAddRemoveMethod();
    addRemoveSensorIface->initialize();

    createSensors();
}

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

void TedSensors::createSensor(const Json& sensorData)
{
    static const Json empty{};

    auto desc = sensorData.value("Desc", empty);
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
                    return;
                }
                auto objPath = sensorDbusPath + sensorType + "/" + name;

                auto tedSensorPtr = std::make_shared<TedSensor>(
                    conn, objPath.c_str(), sensorData, name,
                    unitMap[sensorType]);

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
                tedSensorPtr->setupRead();

                tedSensorsMap.emplace(name, std::move(tedSensorPtr));
                sensorConfigMap.emplace(name, sensorData);
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
        lg2::error("Descriptor for new ted sensor not found in config file");
    }
}

void TedSensors::createSensors()
{
    Json jsonConfigs = parseConfigFile();

    lg2::debug("JSON: {JSON}", "JSON", jsonConfigs.dump());

    for (const auto& j : jsonConfigs)
    {
        createSensor(j);
    }
}

void TedSensors::registerAddRemoveMethod()
{
    addRemoveSensorIface->register_method(
        "AddSensor", [this](std::string sensorName) {
            std::replace(sensorName.begin(), sensorName.end(), ' ', '_');

            if (sensorConfigMap.find(sensorName) == sensorConfigMap.end())
            {
                std::string s =
                    "A sensor named (" + sensorName +
                    ") does not exist in config file, cannot add sensor";
                return s;
            }

            if (tedSensorsMap.find(sensorName) == tedSensorsMap.end())
            {
                createSensor(sensorConfigMap[sensorName]);
                std::string s = "Added ted sensor: " + sensorName;
                return s;
            }
            else
            {
                std::string s = "A sensor named (" + sensorName +
                                ") already exists, cannot add sensor";
                return s;
            }
        });

    addRemoveSensorIface->register_method(
        "RemoveSensor", [this](std::string sensorName) {
            std::replace(sensorName.begin(), sensorName.end(), ' ', '_');

            auto it = tedSensorsMap.find(sensorName);
            if (it != tedSensorsMap.end())
            {
                tedSensorsMap.erase(it);
                std::string s = "Removed ted sensor: " + sensorName;
                return s;
            }
            else
            {
                std::string s = "A sensor named (" + sensorName +
                                ") does not exist, cannot remove sensor";
                return s;
            }
        });
}
} // namespace phosphor::ted_sensor
