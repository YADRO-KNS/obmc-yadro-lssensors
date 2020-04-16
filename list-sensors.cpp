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

using PropertyValue = std::variant<int64_t, std::string, bool>;
using PropertyName = std::string;
using PropertiesMap = std::map<PropertyName, PropertyValue>;

/**
 * @brief Gives a simple access to sensor properties.
 */
class Properties : public PropertiesMap
{
  public:
    using PropertiesMap::PropertiesMap;

    /**
     * @brief Current sensor state
     */
    std::string status() const
    {
        std::string ret = "OK";
        if (getBool("CriticalAlarmLow") || getBool("CriticalAlarmHigh"))
        {
            ret = "Critcal";
        }
        else if (getBool("WarningAlarmLow") || getBool("WarningAlarmHigh"))
        {
            ret = "Warning";
        }

        return ret;
    }

    /**
     * @brief Sensor's scale factor
     */
    float scale() const
    {
        float ret = 1.f;
        auto it = this->find("Scale");
        if (it != this->end())
        {
            auto value = std::get<int64_t>(it->second);
            ret = powf(10, static_cast<float>(value));
        }
        return ret;
    }

    std::string value() const
    {
        return getValue("Value");
    }
    std::string criticalLow() const
    {
        return getValue("CriticalLow");
    }
    std::string criticalHigh() const
    {
        return getValue("CriticalHigh");
    }
    std::string warningLow() const
    {
        return getValue("WarningLow");
    }
    std::string warningHigh() const
    {
        return getValue("WarningHigh");
    }

    /**
     * @brief Short sensors unit name
     */
    std::string unit() const
    {
        std::string ret;

        auto it = this->find("Unit");
        if (it != this->end())
        {
            auto name = std::get<std::string>(it->second);
            name = name.substr(name.rfind('.') + 1);

            if ("Volts" == name)
            {
                ret = "V";
            }
            else if ("DegreesC" == name)
            {
                // The degrees character takes two bytes, but only one place
                // on the screen. It breaks the alignment.
                // Force fit the string to 4 screen characters long.
                ret = "°C  ";
            }
            else if ("Amperes" == name)
            {
                ret = "A";
            }
            else if ("RPMS" == name)
            {
                ret = "RPM";
            }
            else if ("Watts" == name)
            {
                ret = "W";
            }
            else if ("Joules" == name)
            {
                ret = "J";
            }
            else
            {
                ret = name;
            }
        }

        return ret;
    }

  protected:
    /**
     * @brief Check is the specified boolean property has true value
     *
     * @param name - Proprety name
     */
    bool getBool(const PropertyName& name) const
    {
        auto it = this->find(name);
        return (it == this->end() ? false : std::get<bool>(it->second));
    }

    /**
     * @brief Format a sensor value or threshold
     *
     * @param name - Proprety name
     *
     * @return String with formated value of property
     */
    std::string getValue(const PropertyName& name) const
    {
        std::string ret(9, '\0');
        auto it = this->find(name);
        if (it == this->end())
        {
            ret = "N/A";
        }
        else
        {
            auto factor = scale();
            auto value = std::get<int64_t>(it->second);
            if (factor < 1.f)
            {
                snprintf(ret.data(), ret.size(), "%8.03f", value * factor);
            }
            else
            {
                snprintf(ret.data(), ret.size(), "%8d", (int)(value * factor));
            }
        }

        return ret;
    }
};

/**
 * @brief Show sensor's data
 *
 * @param busname - Sensor's object bus name
 * @param path - Sensor's object path
 */
void printSensorData(const std::string& busname, const std::string& path)
{
    // Ask DBus for all sensors properties
    auto m = systemBus.new_method_call(busname.c_str(), path.c_str(),
                                       SYSTEMD_PROPERTIES, "GetAll");
    m.append("");
    auto r = systemBus.call(m);

    if (r.is_method_error())
    {
        fprintf(stderr, "Get properties for %s failed\n", path.c_str());
        return;
    }

    Properties props;
    r.read(props);

    size_t name_pos = path.rfind('/');
    size_t folder_pos = path.rfind('/', name_pos - 1);

    // row format string
    constexpr auto row_fmt = " %-18s %8s %8s %-4s %8s %8s %8s %8s\n";

    // Show group header if it is a new type
    static std::string typeName;

    std::string currentType =
        path.substr(folder_pos + 1, name_pos - folder_pos - 1);
    if (typeName != currentType)
    {
        if (!typeName.empty())
        {
            printf("\n");
        }

        printf("=== %s ===\n", currentType.c_str());
        printf(row_fmt, "Name", "Status", "Value", "Unit", "LC", "LNC", "UNC",
               "UC");
        printf("\n");

        typeName = currentType;
    }

    // Show sensor data
    printf(row_fmt, path.c_str() + name_pos + 1, props.status().c_str(),
           props.value().c_str(), props.unit().c_str(),
           props.criticalLow().c_str(), props.warningLow().c_str(),
           props.warningHigh().c_str(), props.criticalHigh().c_str());
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
            printSensorData(d->first, p->first);
        }
    }

    return EXIT_SUCCESS;
}
