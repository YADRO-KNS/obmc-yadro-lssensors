#include <getopt.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <string>

/**
 * @brief open DBus connection
 *
 * @param host - remote host
 *
 * @return DBus connection object
 */
inline sdbusplus::bus::bus open_system(const char* host = nullptr)
{
    if (!host)
    {
        return sdbusplus::bus::new_system();
    }

    printf("Open DBus session to %s\n", host);
    sd_bus* b = nullptr;
    sd_bus_open_system_remote(&b, host);
    return sdbusplus::bus::bus(b, std::false_type());
}

const std::string prefix = "xyz.openbmc_project.Sensor.Value.Unit.";
const int prefix_len = prefix.length();

/**
 * @brief Returns unit shortname by DBus unit name
 *
 * @param dbus_unit - DBus unut name
 * @return unit shortname
 */
std::string get_unit_shortname(const std::string& dbus_unit)
{
    if (dbus_unit.compare(0, prefix_len, prefix) != 0)
    {
        return "Unknown";
    }

    std::string unit = dbus_unit.substr(prefix_len);

    if ("Volts" == unit)
    {
        return "V";
    }
    else if ("DegreesC" == unit)
    {
        return "\u00B0C"; // UTF-8 Degrees symbol
    }
    else if ("Amperes" == unit)
    {
        return "A";
    }
    else if ("RPMS" == unit)
    {
        return "RPM";
    }
    else if ("Watts" == unit)
    {
        return "W";
    }
    else if ("Joules" == unit)
    {
        return "J";
    }

    return "Unknown";
}

/**
 * @brief Request sensors values
 *
 * @param bus    - DBus connection object
 * @param device - DBus targer who containt sensor
 * @param sensor - Sensors path
 */
void get_sensor_value(sdbusplus::bus::bus& bus, const char* device,
                      const char* sensor)
{
    // --- Ask DBus for all sensors properties
    auto m = bus.new_method_call(device, sensor,
                                 "org.freedesktop.DBus.Properties", "GetAll");
    // NOTE: For this tool may be limited.
    //       For real ALL properties may be requested with empty string.
    m.append("xyz.openbmc_project.Sensor.Value");
    auto r = bus.call(m);

    if (r.is_method_error())
    {
        fprintf(stderr, "Get properties for %s failed\n", sensor);
        return;
    }

    namespace vns = sdbusplus::message::variant_ns;
    using Property = vns::variant<std::string, int64_t, bool>;
    std::map<std::string, Property> d;
    r.read(d);

    // --- Show sensors folder and name ---
    std::string s = sensor;
    size_t name_pos = s.rfind('/');
    size_t folder_pos = s.rfind('/', name_pos - 1);

    printf("%-12s %-16s ",
           std::string(sensor + folder_pos + 1, sensor + name_pos).c_str(),
           sensor + name_pos + 1);

    // --- Show sensors value ---
    s = vns::get<std::string>(d["Unit"]);
    std::string unit = get_unit_shortname(s);

    float scale = powf(10, static_cast<float>(vns::get<int64_t>(d["Scale"])));
    int64_t value = vns::get<int64_t>(d["Value"]);

    if (scale < 1.f)
    {
        printf("%10.03f %s ", value * scale, unit.c_str());
    }
    else
    {
        printf("%10d %s ", (int)(value * scale), unit.c_str());
    }

    // --- End of line ---
    printf("\n");
}

/**
 * @brief compare sensors path with numbers
 */
struct cmp_sensors_name
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        size_t ia = a.find_last_not_of("0123456789") + 1;
        size_t ib = b.find_last_not_of("0123456789") + 1;

        if (ia == ib && ia < a.length() && ib < b.length())
        {
            int r = a.compare(0, ia, b.substr(0, ib));
            if (0 == r)
            {
                return std::stoi(a.substr(ia)) < std::stoi(b.substr(ib));
            }
            return r < 0;
        }
        return a < b;
    }
};
typedef std::map<std::string, std::string, cmp_sensors_name> sensors_t;

/**
 * @brief Application entry point
 *
 * @return
 */
int main(int argc, char* argv[])
{
    const char* host = nullptr;
    bool showhelp = false;
    const struct option opts[] = {{"host", required_argument, nullptr, 'H'},
                                  {"help", no_argument, nullptr, 'h'},
                                  // --- end of array ---
                                  {nullptr, 0, nullptr, '\0'}};

    int c;
    while ((c = getopt_long(argc, argv, "H:h", opts, nullptr)) != -1)
    {
        switch (c)
        {
            case 'H':
                if (optarg)
                {
                    host = optarg;
                }
                else
                {
                    fprintf(stderr, "Remote host required with this option!\n");
                    showhelp = true;
                }
                break;

            case 'h':
                showhelp = true;
                break;
            default:
                fprintf(stderr, "Unknown option found '%c'!\n", c);
                showhelp = true;
                break;
        }
    }

    if (showhelp)
    {
        fprintf(stderr,
                "Usage: %s [options]\n"
                "Options:\n"
                "  -H, --host=[USER@]HOST   Operate on remote host (over ssh)\n"
                "  -h, --help               Show this help\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    auto bus = open_system(host);
    auto method =
        bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetSubTree");
    method.append("/xyz/openbmc_project/sensors", 5,
                  std::vector<std::string>());

    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        fprintf(stderr, "Call GetSubTree() failed\n");
        return EXIT_FAILURE;
    }

    // NOTE: sdbusplus do not allowing std::map with custom predicate
    std::map<std::string, std::map<std::string, std::vector<std::string>>> data;
    reply.read(data);

    // --- Using sorted py path with nums order ---
    std::map<std::string, std::map<std::string, std::vector<std::string>>,
             cmp_sensors_name>
        sorted_data;
    sorted_data.insert(data.begin(), data.end());

    for (auto p = sorted_data.begin(); p != sorted_data.end(); ++p)
    {
        for (auto d = p->second.begin(); d != p->second.end(); ++d)
        {
            get_sensor_value(bus, d->first.c_str(), p->first.c_str());
        }
    }

    return EXIT_SUCCESS;
}
