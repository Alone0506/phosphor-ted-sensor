#include "tedSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <charconv>
#include <iostream>

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

    /* setting available */
    AvailabilityInterface::available(true);
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

void TedSensor::setupRead()
{
    std::filesystem::path simulationFilePath = simulationDirPath / name;

    if (!std::filesystem::exists(simulationFilePath))
    {
        handleResponse(boost::asio::error::not_found, 0);
        return;
    }

    if (!inputDev.is_open())
    {
        inputDev.open(simulationFilePath.string(),
                      boost::asio::random_access_file::read_only);
    }

    std::weak_ptr<TedSensor> weakRef = weak_from_this();
    inputDev.async_read_some_at(
        0, boost::asio::buffer(readBuf),
        [weakRef](const boost::system::error_code& ec, std::size_t bytesRead) {
            std::shared_ptr<TedSensor> self = weakRef.lock();
            if (self)
            {
                self->handleResponse(ec, bytesRead);
            }
        });
}

void TedSensor::restartRead()
{
    std::weak_ptr<TedSensor> weakRef = weak_from_this();
    timer.expires_after(std::chrono::seconds(1));
    timer.async_wait([weakRef](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return; // we're being canceled
        }
        std::shared_ptr<TedSensor> self = weakRef.lock();
        if (!self)
        {
            return;
        }
        self->setupRead();
    });
}

void TedSensor::handleResponse(const boost::system::error_code& err,
                               std::size_t bytesRead)
{
    if (err == boost::system::errc::bad_file_descriptor)
    {
        lg2::error("Sensor '{NAME}' bad file descriptor", "NAME", name);
        return; // we're being destroyed
    }

    if (err == boost::asio::error::misc_errors::not_found)
    {
        lg2::warning("Sensor '{NAME}' file not found, retrying...", "NAME",
                     name);
        restartRead();
        return;
    }

    double value = std::numeric_limits<double>::quiet_NaN();

    if (!err)
    {
        const char* bufEnd = readBuf.data() + bytesRead;
        double nvalue = 0;
        auto [ptr, ec] = std::from_chars(readBuf.data(), bufEnd, nvalue);
        if (ec == std::errc())
        {
            value = std::clamp(nvalue, ValueIface::minValue(),
                               ValueIface::maxValue());
        }
    }
    else
    {
        lg2::error("Sensor '{NAME}' read error: {ERROR}", "NAME", name, "ERROR",
                   err.message());
    }

    // Avoid unnecessary D-Bus property-changed signal when value hasn't changed
    double oldValue = ValueIface::value();
    if (!((std::isnan(oldValue) && std::isnan(value)) || oldValue == value))
    {
        ValueIface::value(value);
        checkThreshold();
    }

    restartRead();
}

} // namespace phosphor::ted_sensor
