#include "tedSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

static constexpr std::string sensorDbusPath = "/xyz/openbmc_project/sensors/";

std::filesystem::path sensorDirPath = "/tmp/ted_sensor/sensors";
std::filesystem::path simulationDirPath = "/tmp/ted_sensor/simulation";

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

void TedSensor::createThresholds(const Json& threshold,
                                 const std::string& objPath)
{
    if (threshold.empty())
    {
        return;
    }

    if (threshold.contains("CriticalHigh") || threshold.contains("CriticalLow"))
    {
        criticalIface =
            std::make_unique<Threshold<CriticalObject>>(bus, objPath.c_str());

        if (threshold.contains("CriticalHigh"))
        {
            criticalIface->criticalHigh(threshold.value(
                "CriticalHigh", std::numeric_limits<double>::quiet_NaN()));
        }
        if (threshold.contains("CriticalLow"))
        {
            criticalIface->criticalLow(threshold.value(
                "CriticalLow", std::numeric_limits<double>::quiet_NaN()));
        }
    }

    if (threshold.contains("WarningHigh") || threshold.contains("WarningLow"))
    {
        warningIface =
            std::make_unique<Threshold<WarningObject>>(bus, objPath.c_str());

        if (threshold.contains("WarningHigh"))
        {
            warningIface->warningHigh(threshold.value(
                "WarningHigh", std::numeric_limits<double>::quiet_NaN()));
        }
        if (threshold.contains("WarningLow"))
        {
            warningIface->warningLow(threshold.value(
                "WarningLow", std::numeric_limits<double>::quiet_NaN()));
        }
    }
}

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

AssociationList getAssociationsFromJson(const Json& j)
{
    AssociationList assocs{};
    try
    {
        j.get_to(assocs);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to parse association: {ERROR}", "ERROR", e);
    }
    return assocs;
}

void TedSensor::updateTedSensor()
{
    double value = std::numeric_limits<double>::quiet_NaN();

    std::filesystem::path simulationFilePath = simulationDirPath / name;
    if (std::filesystem::exists(simulationFilePath))
    {
        std::ifstream simFile(simulationFilePath);
        if (simFile.is_open())
        {
            simFile >> value;
            simFile.close();
        }
        else
        {
            lg2::error("Failed to open simulation file {FILE}", "FILE",
                       simulationFilePath.string());
        }
    }

    value = std::clamp(value, ValueIface::minValue(), ValueIface::maxValue());
    ValueIface::value(value);

    std::filesystem::path sensorFilePath = sensorDirPath / name;
    if (std::filesystem::exists(sensorFilePath))
    {
        std::ofstream sensorFile(sensorFilePath);
        if (sensorFile.is_open())
        {
            sensorFile << value;
            sensorFile.close();
        }
        else
        {
            lg2::error("Failed to open sensor file {FILE}", "FILE",
                       sensorFilePath.string());
        }
    }
}

void TedSensor::initTedSensor(const Json& sensorConfig,
                              const std::string& objPath,
                              const std::string& sensorType)
{
    static const Json empty{};

    /* Setting unit value */
    ValueIface::unit(unitMap[sensorType]);

    /* get threshold values if defined in Threshold */
    auto thresholdConf = sensorConfig.value("Threshold", empty);
    createThresholds(thresholdConf, objPath);

    /* get MaxValue, MinValue setting if defined in Desc */
    auto descConf = sensorConfig.value("Desc", empty);
    auto maxConf = descConf.find("MaxValue");
    if (maxConf != descConf.end() && maxConf->is_number())
    {
        ValueIface::maxValue(maxConf->get<double>());
    }

    auto minConf = descConf.find("MinValue");
    if (minConf != descConf.end() && minConf->is_number())
    {
        ValueIface::minValue(minConf->get<double>());
    }

    /* get associations if defined in Associations */
    auto assocConf = sensorConfig.value("Associations", empty);
    if (!assocConf.empty())
    {
        AssociationList assocs = getAssociationsFromJson(assocConf);
        if (!assocs.empty())
        {
            associationIface =
                std::make_unique<AssociationObject>(bus, objPath.c_str());
            associationIface->associations(assocs);
        }
    }
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

void TedSensors::createTedSensors()
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
                        bus, objPath.c_str(), j, name, sensorType);

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

                    // TODO: 每一秒polling一次, 用nvme的event timer
                    tedSensorPtr->updateTedSensor();

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

} // namespace phosphor::ted_sensor
