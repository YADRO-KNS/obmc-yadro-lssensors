#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <string>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

typedef std::vector<std::string> strarray;

static auto bus = sdbusplus::bus::new_system();

/**
 * @brief Request sensors values
 *
 * @param device - DBus targer who containt sensor
 * @param sensor - Sensors path
 */
void get_sensor_value(const char *device, const char *sensor)
{
    // --- Ask DBus for all sensors properties
    auto m = bus.new_method_call(device,
                                 sensor,
                                 "org.freedesktop.DBus.Properties",
                                 "GetAll");
    // NOTE: For this tool may be limited.
    //       For real ALL properties may be requested with empty string.
    m.append("xyz.openbmc_project.Sensor.Value");
    auto r = bus.call(m);

    if (r.is_method_error())
    {
        fprintf(stderr, "Get properties for %s failed\n", sensor);
        return;
    }

    std::map<std::string, sdbusplus::message::variant<std::string, int64_t, bool> > d;
    r.read(d);

    // --- Show sensors folder and name ---
    std::string s = sensor;
    size_t name_pos = s.rfind('/');
    size_t folder_pos = s.rfind('/', name_pos - 1);

    printf("%-12s %-12s ",
           std::string(sensor + folder_pos + 1, sensor + name_pos).c_str(),
           sensor + name_pos + 1);

    // --- Show sensors value ---
    s = d["Unit"].get<std::string>();
    std::string unit;
    if (s.back() == 'C')
        unit = "\u00B0C"; // UTF-8 Degrees symbol
    else if (s.back() == 's')
        unit = "V";
    else if (s.back() == 'S')
        unit = "RPM";

    float   scale = powf(10, (float)d["Scale"].get<int64_t>());
    int64_t value = d["Value"].get<int64_t>();

    if (scale < 1.f)
        printf("%10.03f %s ", value * scale, unit.c_str());
    else
        printf("%10d %s ", (int)(value * scale), unit.c_str());

    // --- End of line ---
    printf("\n");
}
/**
 * @brief Application entry point
 *
 * @return
 */
int main(void)
{
    auto method = bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                      "/xyz/openbmc_project/object_mapper",
                                      "xyz.openbmc_project.ObjectMapper",
                                      "GetSubTree");
    method.append("/xyz/openbmc_project/sensors", 5, strarray());

    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        fprintf(stderr, "Call GetSubTree() failed\n");
        return EXIT_FAILURE;
    }

    std::map<std::string, std::map<std::string, strarray> > data;
    reply.read(data);

    for (auto p = data.begin(); p != data.end(); ++p)
    {
        for (auto d = p->second.begin(); d != p->second.end(); ++d)
            get_sensor_value(d->first.c_str(), p->first.c_str());
    }

    return 0;
}
