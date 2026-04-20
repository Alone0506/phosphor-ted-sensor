#pragma once

#include <boost/asio/random_access_file.hpp>
#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Availability/server.hpp>

#include <array>
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

class TedSensor :
    public TedIface,
    public std::enable_shared_from_this<TedSensor>
{
  public:
    TedSensor() = delete;
    virtual ~TedSensor() = default;

    /** @brief Constructs TedSensor
     *
     * @param[in] conn         - Handle to system dbus
     * @param[in] objPath      - The Dbus path of sensor
     * @param[in] sensorConfig - Json object for sensor config
     * @param[in] name         - Sensor name
     * @param[in] sensorUnit   - sensor unit
     */
    TedSensor(std::shared_ptr<sdbusplus::asio::connection>& conn,
              const char* objPath, const Json& sensorConfig,
              const std::string& name, const ValueIface::Unit& sensorUnit) :
        TedIface(*conn, objPath, action::defer_emit), name(name),
        inputDev(conn->get_io_context()), timer(conn->get_io_context())
    {
        initTedSensor(sensorConfig, sensorUnit);
    }

    void setupRead();

  private:
    std::string name;
    std::array<char, 128> readBuf;
    boost::asio::random_access_file inputDev;
    boost::asio::steady_timer timer;

    void initTedSensor(const Json& sensorConfig,
                       const ValueIface::Unit& sensorUnit);
    void checkThreshold();
    void handleResponse(const boost::system::error_code& err,
                        std::size_t bytesRead);
    void restartRead();
};

} // namespace phosphor::ted_sensor
