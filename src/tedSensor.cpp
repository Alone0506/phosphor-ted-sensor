#include "tedSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

std::filesystem::path simulationDirPath = "/tmp/sensor/simulation";

// see phosphor-logging/lib/include/phosphor-logging/lg2.hpp
PHOSPHOR_LOG2_USING_WITH_FLAGS;

namespace phosphor::ted_sensor
{
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

void TedSensor::initTedSensor(const Json& sensorConfig,
                              const ValueIface::Unit& sensorUnit)
{
    static const Json empty{};

    /* Setting unit of value */
    ValueIface::unit(sensorUnit);

    /* get threshold values if defined in Threshold */
    auto thresholdConf = sensorConfig.value("Threshold", empty);
    double w = thresholdConf.value("WarningHigh",
                                   std::numeric_limits<double>::quiet_NaN());
    double q = thresholdConf.value("WarningLow",
                                   std::numeric_limits<double>::quiet_NaN());
    double r = thresholdConf.value("CriticalHigh",
                                   std::numeric_limits<double>::quiet_NaN());
    double s = thresholdConf.value("CriticalLow",
                                   std::numeric_limits<double>::quiet_NaN());
    lg2::info("WarningHigh: {W}, WarningLow: {Q}, CriticalHigh: {R}, CriticalLow: {S}", "W", w, "Q", q, "R", r, "S", s);
    WarningIface::warningHigh(thresholdConf.value(
        "WarningHigh", std::numeric_limits<double>::quiet_NaN()));
    WarningIface::warningLow(thresholdConf.value(
        "WarningLow", std::numeric_limits<double>::quiet_NaN()));
    CriticalIface::criticalHigh(thresholdConf.value(
        "CriticalHigh", std::numeric_limits<double>::quiet_NaN()));
    CriticalIface::criticalLow(thresholdConf.value(
        "CriticalLow", std::numeric_limits<double>::quiet_NaN()));

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
            AssociationIface::associations(assocs);
        }
    }

    /* Setting available */
    AvailabilityInterface::available(true);
}

void TedSensor::read()
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
}

void TedSensor::checkThreshold()
{
    double value = ValueIface::value();

    // warning
    double warningHigh = WarningIface::warningHigh();
    double warningLow = WarningIface::warningLow();
    bool oldWarningAlarmHigh = WarningIface::warningAlarmHigh();
    bool oldWarningAlarmLow = WarningIface::warningAlarmLow();
    bool newWarningAlarmHigh = (value >= warningHigh);
    bool newWarningAlarmLow = (value <= warningLow);

    WarningIface::warningAlarmHigh(newWarningAlarmHigh);
    if (oldWarningAlarmHigh != newWarningAlarmHigh)
    {
        if (newWarningAlarmHigh)
        {
            WarningIface::warningHighAlarmAsserted(value);
        }
        else
        {
            WarningIface::warningHighAlarmDeasserted(value);
        }
    }

    WarningIface::warningAlarmLow(newWarningAlarmLow);
    if (oldWarningAlarmLow != newWarningAlarmLow)
    {
        if (newWarningAlarmLow)
        {
            WarningIface::warningLowAlarmAsserted(value);
        }
        else
        {
            WarningIface::warningLowAlarmDeasserted(value);
        }
    }

    // critical
    double criticalHigh = CriticalIface::criticalHigh();
    double criticalLow = CriticalIface::criticalLow();
    bool oldCriticalAlarmHigh = CriticalIface::criticalAlarmHigh();
    bool oldCriticalAlarmLow = CriticalIface::criticalAlarmLow();
    bool newCriticalAlarmHigh = (value >= criticalHigh);
    bool newCriticalAlarmLow = (value <= criticalLow);

    CriticalIface::criticalAlarmHigh(newCriticalAlarmHigh);
    if (oldCriticalAlarmHigh != newCriticalAlarmHigh)
    {
        if (newCriticalAlarmHigh)
        {
            CriticalIface::criticalHighAlarmAsserted(value);
        }
        else
        {
            CriticalIface::criticalHighAlarmDeasserted(value);
        }
    }

    CriticalIface::criticalAlarmLow(newCriticalAlarmLow);
    if (oldCriticalAlarmLow != newCriticalAlarmLow)
    {
        if (newCriticalAlarmLow)
        {
            CriticalIface::criticalLowAlarmAsserted(value);
        }
        else
        {
            CriticalIface::criticalLowAlarmDeasserted(value);
        }
    }
}

} // namespace phosphor::ted_sensor
