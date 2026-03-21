#pragma once

#include "tedSensor.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <string>
#include <unordered_map>

namespace phosphor::ted_sensor
{

class TedSensors
{
  public:
    TedSensors() = delete;
    virtual ~TedSensors() = default;

    /**
     * @brief Constructs TedSensors
     *
     * @param[in] conn - D-Bus object connection
     */
    explicit TedSensors(std::shared_ptr<sdbusplus::asio::connection>& conn);

  private:
    std::shared_ptr<sdbusplus::asio::connection>& conn;
    sdbusplus::asio::object_server objServer;
    sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic> _timer;

    std::shared_ptr<sdbusplus::asio::dbus_interface> addRemoveSensorIface;

    /** @brief Map of the object TedSensor */
    std::unordered_map<std::string, std::unique_ptr<TedSensor>> tedSensorsMap;
    /** @brief JSON configuration, key: sensor name, value: sensor config */
    std::unordered_map<std::string, Json> sensorConfigMap;

    void registerAddRemoveMethod();

    /** @brief Parsing ted sensor configuration */
    Json parseConfigFile();
    /** @brief Create list of sensors from JSON config */
    void createSensor(const Json& sensorData);
    void createSensors();

    void read();
};

} // namespace phosphor::ted_sensor
