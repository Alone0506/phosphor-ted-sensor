#pragma once

#include <phosphor-logging/commit.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>

namespace phosphor::ted_sensor
{

template <typename... T>
using ServerObject = typename sdbusplus::server::object_t<T...>;

namespace threshold_ns =
    sdbusplus::xyz::openbmc_project::Sensor::Threshold::server;
using Unit = sdbusplus::xyz::openbmc_project::Sensor::server::Value::Unit;
using CriticalObject = ServerObject<threshold_ns::Critical>;
using WarningObject = ServerObject<threshold_ns::Warning>;

template <typename T>
struct Threshold;
template <>
struct Threshold<WarningObject> : public WarningObject
{
    static constexpr auto name = "Warning";
    using WarningObject::WarningObject;

    sdbusplus::bus_t& bus;
    std::string objPath;
    Unit units;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attaach to.
     *  @param[in] path - Path to attach at.
     *  @param[in] units - units
     */
    Threshold(sdbusplus::bus_t& bus, const char* path, Unit units) :
        WarningObject(bus, path), bus(bus), objPath(std::string(path)),
        units(units)
    {}

    auto high()
    {
        return WarningObject::warningHigh();
    }

    auto low()
    {
        return WarningObject::warningLow();
    }

    template <typename... Args>
    auto alarmHigh(Args... args)
    {
        return warningAlarmHigh(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLow(Args... args)
    {
        return warningAlarmLow(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmHighSignalAsserted(Args... args)
    {
        // TODO: 要開啟 Sel logger的 warningHighAlarmAsserted 功能才會記錄 log
        return warningHighAlarmAsserted(std::forward<Args>(args)...);
    }

    // 允許傳遞或不傳遞sensorValue
    template <typename... Args>
    auto alarmHighSignalDeasserted(Args... args)
    {
        return warningHighAlarmDeasserted(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLowSignalAsserted(Args... args)
    {
        return warningLowAlarmAsserted(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLowSignalDeasserted(Args... args)
    {
        return warningLowAlarmDeasserted(std::forward<Args>(args)...);
    }

    /** @brief Set value of warningHigh */
    virtual double warningHigh(double value)
    {
        return WarningObject::warningHigh(value);
    }

    /** @brief Set value of warningLow */
    virtual double warningLow(double value)
    {
        return WarningObject::warningLow(value);
    }
};

template <>
struct Threshold<CriticalObject> : public CriticalObject
{
    static constexpr auto name = "Critical";
    using CriticalObject::CriticalObject;

    sdbusplus::bus_t& bus;
    std::string objPath;
    Unit units;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attaach to.
     *  @param[in] path - Path to attach at.
     *  @param[in] units - units
     */
    Threshold(sdbusplus::bus_t& bus, const char* path, Unit units) :
        CriticalObject(bus, path), bus(bus), objPath(std::string(path)),
        units(units)
    {}

    auto high()
    {
        return CriticalObject::criticalHigh();
    }

    auto low()
    {
        return CriticalObject::criticalLow();
    }

    template <typename... Args>
    auto alarmHigh(Args... args)
    {
        return criticalAlarmHigh(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLow(Args... args)
    {
        return criticalAlarmLow(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmHighSignalAsserted(Args... args)
    {
        // TODO: 要開啟 Sel logger的 criticalHighAlarmAsserted 功能才會記錄 log
        return criticalHighAlarmAsserted(std::forward<Args>(args)...);
    }

    // 允許傳遞或不傳遞sensorValue
    template <typename... Args>
    auto alarmHighSignalDeasserted(Args... args)
    {
        return criticalHighAlarmDeasserted(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLowSignalAsserted(Args... args)
    {
        return criticalLowAlarmAsserted(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto alarmLowSignalDeasserted(Args... args)
    {
        return criticalLowAlarmDeasserted(std::forward<Args>(args)...);
    }

    /** @brief Set value of warningHigh */
    virtual double warningHigh(double value)
    {
        return CriticalObject::criticalHigh(value);
    }

    /** @brief Set value of warningLow */
    virtual double warningLow(double value)
    {
        return CriticalObject::criticalLow(value);
    }
};

} // namespace phosphor::ted_sensor
