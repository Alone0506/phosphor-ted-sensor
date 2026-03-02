#pragma once

#include "tedSensor.hpp"

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
     * @param[in] bus - Handle to system dbus
     */
    explicit TedSensors(sdbusplus::bus_t& bus) :
        bus(bus), _event(sdeventplus::Event::get_default()),
        _timer(_event, std::bind(&TedSensors::read, this))
    {
        createSensors();
    }

    void run();

  private:
    /** @brief sdbusplus bus client connection. */
    sdbusplus::bus_t& bus;

    /** @brief the Event Loop structure */
    sdeventplus::Event _event;
    /** @brief Read Timer */
    sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic> _timer;

    /** @brief Parsing ted sensor configuration */
    Json parseConfigFile();

    /** @brief Map of the object TedSensor */
    std::unordered_map<std::string, std::unique_ptr<TedSensor>> tedSensorsMap;

    /** @brief Create list of virtual sensors from JSON config */
    void createSensors();

    void read();
};

} // namespace phosphor::ted_sensor
