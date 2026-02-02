#include "tedSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

std::filesystem::path sensorDbusPath = "/xyz/openbmc_project/sensors/";

std::filesystem::path simulationDirPath = "/tmp/ted-sensor/simulation";

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

void TedSensor::createThresholds(
    const Json& threshold, const std::string& objPath, ValueIface::Unit units)
{
    if (threshold.empty())
    {
        return;
    }

    if (threshold.contains("CriticalHigh") || threshold.contains("CriticalLow"))
    {
        criticalIface = std::make_unique<Threshold<CriticalObject>>(
            bus, objPath.c_str(), units);

        criticalIface->criticalHigh(threshold.value(
            "CriticalHigh", std::numeric_limits<double>::quiet_NaN()));
        criticalIface->criticalLow(threshold.value(
            "CriticalLow", std::numeric_limits<double>::quiet_NaN()));
    }

    if (threshold.contains("WarningHigh") || threshold.contains("WarningLow"))
    {
        warningIface = std::make_unique<Threshold<WarningObject>>(
            bus, objPath.c_str(), units);

        warningIface->warningHigh(threshold.value(
            "WarningHigh", std::numeric_limits<double>::quiet_NaN()));
        warningIface->warningLow(threshold.value(
            "WarningLow", std::numeric_limits<double>::quiet_NaN()));
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
    else
    {
        value = 55.3;
    }

    value = std::clamp(value, ValueIface::minValue(), ValueIface::maxValue());
    ValueIface::value(value);

    std::filesystem::path sensorFilePath = sensorDbusPath / name;
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

    units = unitMap.at(sensorType);

    /* get threshold values if defined in config */
    auto thresholds = sensorConfig.value("Thresholds", empty);
    createThresholds(thresholds, objPath, units);

    /* get MaxValue, MinValue setting if defined in config */
    auto confDesc = sensorConfig.value("Desc", empty);
    if (auto maxConf = confDesc.find("MaxValue");
        maxConf != confDesc.end() && maxConf->is_number())
    {
        ValueIface::maxValue(maxConf->get<double>());
    }
    if (auto minConf = confDesc.find("MinValue");
        minConf != confDesc.end() && minConf->is_number())
    {
        ValueIface::minValue(minConf->get<double>());
    }

    /* get association */
    auto assocJson = sensorConfig.value("Associations", empty);
    if (!assocJson.empty())
    {
        auto assocs = getAssociationsFromJson(assocJson);
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
                    // TODO: 每一秒polling一次
                    tedSensorPtr->updateTedSensor();

                    /* Initialize unit value */
                    tedSensorPtr->ValueIface::unit(unitMap[sensorType]);
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

} // namespace phosphor::ted_sensor
