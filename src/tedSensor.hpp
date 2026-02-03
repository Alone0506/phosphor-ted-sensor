#pragma once

#include "threshold.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>

#include <map>
#include <string>

namespace phosphor::ted_sensor
{

// see phosphor-logging/lib/include/phosphor-logging/lg2.hpp
PHOSPHOR_LOG2_USING_WITH_FLAGS;

using BasicVariantType =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<std::string>>;

using PropertyMap = std::map<std::string, BasicVariantType>;

using InterfaceMap = std::map<std::string, PropertyMap>;

using ManagedObjectType =
    std::map<sdbusplus::message::object_path, InterfaceMap>;

using Json = nlohmann::json;

template <typename... T>
using ServerObject = typename sdbusplus::server::object_t<T...>;

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using ValueObject = ServerObject<ValueIface>;

using AssociationIface =
    sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using AssociationObject = ServerObject<AssociationIface>;

class TedSensor : public ValueObject
{
  public:
    TedSensor() = delete;
    virtual ~TedSensor() = default;

    /** @brief Constructs TedSensor
     *
     * @param[in] bus          - Handle to system dbus
     * @param[in] objPath      - The Dbus path of sensor
     * @param[in] sensorConfig - Json object for sensor config
     * @param[in] name         - Sensor name
     * @param[in] sensorType   - sensor type(unit)
     */
    TedSensor(sdbusplus::bus_t& bus, const char* objPath,
              const Json& sensorConfig, const std::string& name,
              const std::string& sensorType) :
        ValueObject(bus, objPath, action::defer_emit), bus(bus), name(name)
    {
        initTedSensor(sensorConfig, objPath, sensorType);
    }

    /** @brief Set sensor value */
    void setSensorValue(double value);

  private:
    /** @brief sdbusplus bus client connection */
    sdbusplus::bus_t& bus;
    /** @brief name of sensor */
    std::string name;

    /** @brief critical threshold interface object */
    std::unique_ptr<Threshold<CriticalObject>> criticalIface;
    /** @brief warning threshold interface object */
    std::unique_ptr<Threshold<WarningObject>> warningIface;

    /** @brief association interface object */
    std::unique_ptr<AssociationObject> associationIface;

    /** @brief read config from json object and initialize sensor data */
    void initTedSensor(const Json& sensorConfig, const std::string& objPath,
                       const std::string& sensorType);

    /** @brief create threshold objects from json config */
    void createThresholds(const Json& threshold, const std::string& objPath,
                          ValueIface::Unit units);

    /** @brief Update sensor at regular interval */
    void updateTedSensor();

    /** @brief Check Sensor threshold and update alarm and log. Returns
     * true if the threshold range has no alarms set. change will be
     * set if a change to the alarms were detected, else will be left
     * unchanged */
    template <typename V, typename T>
    bool checkThresholds(V value, T& threshold, bool& change)
    {
        if (!threshold)
            return true;

        static constexpr auto tname = T::element_type::name;

        auto alarmHigh = threshold->alarmHigh();
        if ((!alarmHigh && value >= threshold->high()) ||
            (alarmHigh && value < threshold->high()))
        {
            change = true;
            if (!alarmHigh)
            {
                lg2::error(
                    "ASSERT: sensor {SENSOR} is above the high threshold "
                    "{THRESHOLD}.",
                    "SENSOR", name, "THRESHOLD", tname);
                threshold->alarmHighAsserted(value);
            }
            else
            {
                lg2::info(
                    "DEASSERT: sensor {SENSOR} is below the high threshold "
                    "{THRESHOLD}.",
                    "SENSOR", name, "THRESHOLD", tname);
                threshold->alarmHighDeasserted(value);
            }
            alarmHigh = !alarmHigh;
            threshold->alarmHigh(alarmHigh);
        }

        auto alarmLow = threshold->alarmLow();
        if ((!alarmLow && value <= threshold->low()) ||
            (alarmLow && value > (threshold->low())))
        {
            change = true;
            if (!alarmLow)
            {
                lg2::error("ASSERT: sensor {SENSOR} is below the low threshold "
                           "{THRESHOLD}.",
                           "SENSOR", name, "THRESHOLD", tname);
                threshold->alarmLowAsserted(value);
            }
            else
            {
                lg2::info(
                    "DEASSERT: sensor {SENSOR} is above the low threshold "
                    "{THRESHOLD}.",
                    "SENSOR", name, "THRESHOLD", tname);
                threshold->alarmLowDeasserted(value);
            }
        }
    }
};

class TedSensors
{
  public:
    TedSensors() = delete;
    virtual ~TedSensors() = default;

    /**
     * @brief Constructs TedSensors
     *
     * @param[in] bus     - Handle to system dbus
     */
    explicit TedSensors(sdbusplus::but_t& bus) : bus(bus)
    {
        createTedSensors();
    }

  private:
    /** @brief sdbusplus bus client connection. */
    sdbusplus::bus_t& bus;

    /** @brief Parsing ted sensor configuration */
    Json parseConfigFile();

    /** @brief Map of the object TedSensor */
    std::unordered_map<std::string, std::unique_ptr<TedSensor>> tedSensorsMap;

    /** @brief Create list of virtual sensors from JSON config */
    void createTedSensors();
};

} // namespace phosphor::ted_sensor
