#pragma once

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>

#include <string>

namespace phosphor::ted_sensor
{
using Json = nlohmann::json;

using ValueIface = sdbusplus::server::xyz::openbmc_project::sensor::Value;

using CriticalIface =
    sdbusplus::server::xyz::openbmc_project::sensor::threshold::Critical;

using WarningIface =
    sdbusplus::server::xyz::openbmc_project::sensor::threshold::Warning;

using AvailabilityInterface =
    sdbusplus::server::xyz::openbmc_project::state::decorator::Availability;

using AssociationIface =
    sdbusplus::server::xyz::openbmc_project::association::Definitions;

template <typename... T>
using ServerObject = typename sdbusplus::server::object_t<T...>;
using TedIface = ServerObject<ValueIface, CriticalIface, WarningIface,
                              AvailabilityInterface, AssociationIface>;

class TedSensor : public TedIface
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
     * @param[in] sensorUnit   - sensor unit
     */
    TedSensor(sdbusplus::bus_t& bus, const char* objPath,
              const Json& sensorConfig, const std::string& name,
              const ValueIface::Unit& sensorUnit) :
        TedIface(bus, objPath, action::defer_emit), bus(bus), name(name)
    {
        initTedSensor(sensorConfig, sensorUnit);
    }

    /** @brief Update sensor at regular interval */
    void read();

    void checkThreshold();

  private:
    /** @brief sdbusplus bus client connection */
    sdbusplus::bus_t& bus;
    /** @brief name of sensor */
    std::string name;

    /** @brief read config from json object and initialize sensor data */
    void initTedSensor(const Json& sensorConfig,
                       const ValueIface::Unit& sensorUnit);
};

} // namespace phosphor::ted_sensor
