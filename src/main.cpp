#include "tedSensors.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/server.hpp>

int main()
{
    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name("xyz.openbmc_project.TedSensor");
    phosphor::ted_sensor::TedSensors tedSensors(conn);

    io.run();
    return 0;
}
