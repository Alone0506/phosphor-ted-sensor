#include "tedSensor.hpp"

#include <sdbusplus/server.hpp>

int main()
{
    // Get a handle to system dbus
    auto bus = sdbusplus::bus::new_default();

    // Add the ObjectManager interface
    sdbusplus::server::manager_t objManager(bus,
                                            "/xyz/openbmc_project/sensors");

    // Create an ted sensor object
    phosphor::ted_sensor::TedSensors tedSensors(bus);

    // Request service bus name
    bus.request_name("xyz.openbmc_project.TedSensor");

    // Run the dbus loop.
    bus.process_loop();

    return 0;
}