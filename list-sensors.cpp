#include "config.h"

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

// Bus handler singleton
static sdbusplus::bus::bus systemBus = sdbusplus::bus::new_default();

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
 * @param busname - Sensor's object bus name
 * @param path - Sensor's object path
 */
void get_sensor_value(const char* busname, const char* path)
{
    // --- Ask DBus for all sensors properties
    auto m =
        systemBus.new_method_call(busname, path, SYSTEMD_PROPERTIES, "GetAll");
    // NOTE: For this tool may be limited.
    //       For real ALL properties may be requested with empty string.
    m.append(SENSOR_VALUE_IFACE);
    auto r = systemBus.call(m);

    if (r.is_method_error())
    {
        fprintf(stderr, "Get properties for %s failed\n", path);
        return;
    }

    namespace vns = sdbusplus::message::variant_ns;
    using Property = vns::variant<std::string, int64_t, bool>;
    std::map<std::string, Property> d;
    r.read(d);

    // --- Show sensors folder and name ---
    std::string s = path;
    size_t name_pos = s.rfind('/');
    size_t folder_pos = s.rfind('/', name_pos - 1);

    printf("%-12s %-16s ",
           std::string(path + folder_pos + 1, path + name_pos).c_str(),
           path + name_pos + 1);

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
        constexpr auto digits = "0123456789";
        size_t ia = a.find_last_not_of(digits) + 1;
        size_t ib = b.find_last_not_of(digits) + 1;

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
#ifdef WITH_REMOTE_HOST
    const char* host = nullptr;
#endif
    bool showhelp = false;
    const struct option opts[] = {
#ifdef WITH_REMOTE_HOST
        {"host", required_argument, nullptr, 'H'},
#endif
        {"help", no_argument, nullptr, 'h'},
        // --- end of array ---
        {nullptr, 0, nullptr, '\0'}};

    int c;
#ifdef WITH_REMOTE_HOST
    while ((c = getopt_long(argc, argv, "H:h", opts, nullptr)) != -1)
#else
    while ((c = getopt_long(argc, argv, "h", opts, nullptr)) != -1)
#endif
    {
        switch (c)
        {
#ifdef WITH_REMOTE_HOST
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
#endif
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
                "Usage: %s [options] [sensors-type]\n"
                "  Shows all sensors of the specified type.\n"
                "  If the type is not specified shows all found sensors.\n"
                "Options:\n"
#ifdef WITH_REMOTE_HOST
                "  -H, --host=[USER@]HOST   Operate on remote host (over ssh)\n"
#endif
                "  -h, --help               Show this help\n",
                argv[0]);
        return EXIT_FAILURE;
    }

#ifdef WITH_REMOTE_HOST
    if (host)
    {
        printf("Open DBus session to %s\n", host);
        sd_bus* b = nullptr;
        sd_bus_open_system_remote(&b, host);
        systemBus = sdbusplus::bus::bus(b, std::false_type());
    }
#endif

    std::string root_path = SENSORS_PATH;
    if (optind < argc)
    {
        const size_t len = strlen(argv[optind]);
        for (size_t i = 0; i < len; ++i)
        {
            const auto& c = argv[optind][i];
            if (!isalnum(c) && c != '_')
            {
                fprintf(stderr, "Invalid sensor type is specified!\n");
                return EXIT_FAILURE;
            }
        }

        root_path += "/";
        root_path += argv[optind];
    }

    auto method = systemBus.new_method_call(MAPPER_BUS, MAPPER_PATH,
                                            MAPPER_IFACE, "GetSubTree");
    const std::vector<std::string> ifaces = {SENSOR_VALUE_IFACE};
    method.append(root_path, 0, ifaces);

    auto reply = systemBus.call(method);
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
            get_sensor_value(d->first.c_str(), p->first.c_str());
        }
    }

    return EXIT_SUCCESS;
}
