#include "tedSensors.hpp"

#include <sdbusplus/server.hpp>

int main()
{
    // Get a handle to system dbus
    auto bus = sdbusplus::bus::new_default();

    sd_event* event = nullptr;
    auto eventDeleter = [](sd_event* e) { e = sd_event_unref(e); };
    using SdEvent = std::unique_ptr<sd_event, decltype(eventDeleter)>;
    // acquire a reference to the default event loop
    sd_event_default(&event);
    SdEvent sdEvent{event, eventDeleter};
    event = nullptr;
    // attach bus to this event loop
    bus.attach_event(sdEvent.get(), SD_EVENT_PRIORITY_NORMAL);

    // Add the ObjectManager interface
    sdbusplus::server::manager_t objManager(bus,
                                            "/xyz/openbmc_project/sensors");

    // Create an ted sensor object
    phosphor::ted_sensor::TedSensors tedSensors(bus);

    // Request service bus name
    bus.request_name("xyz.openbmc_project.TedSensor");

    tedSensors.run();

    // Start event loop for all sd-bus events
    sd_event_loop(bus.get_event());

    bus.detach_event();

    return 0;
}