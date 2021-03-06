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

using PropertyValue = std::variant<int64_t, std::string, bool, double>;
using PropertyName = std::string;
using PropertiesMap = std::map<PropertyName, PropertyValue>;

static constexpr auto SYSTEMD_PROPERTIES = "org.freedesktop.DBus.Properties";

/**
 * @brief Gives a simple access to sensor properties.
 */
class Properties : public PropertiesMap
{
  public:
    using PropertiesMap::PropertiesMap;

    /**
     * @brief Check if sensor Available and Functional
     */
    std::string functional() const
    {
        std::string ret = "OK";
        auto it = this->find("Functional");
        if (it != this->end() && std::get<bool>(it->second) == false)
        {
            ret = "FAIL";
        }
        it = this->find("Available");
        if (it != this->end() && std::get<bool>(it->second) == false)
        {
            ret = "N/A";
        }

        return ret;
    }

    /**
     * @brief Current sensor state
     */
    std::string status() const
    {
        std::string ret = functional();
        if ("OK" == ret)
        {
            if (getBool("FatalAlarmHigh"))
            {
                ret = "Fatal";
            }
            else if (getBool("CriticalAlarmLow") || getBool("CriticalAlarmHigh"))
            {
                ret = "Critical";
            }
            else if (getBool("WarningAlarmLow") || getBool("WarningAlarmHigh"))
            {
                ret = "Warning";
            }
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
        if ("OK" != functional())
        {
            return "N/A";
        }
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
    std::string fatalHigh() const
    {
        return getValue("FatalHigh");
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
            else if ("Meters" == name)
            {
                ret = "m";
            }
            else if ("Percent" == name)
            {
                ret = "%";
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
     * @param name - Property name
     */
    bool getBool(const PropertyName& name) const
    {
        auto it = this->find(name);
        return (it == this->end() ? false : std::get<bool>(it->second));
    }

    /**
     * @brief Format a sensor value or threshold
     *
     * @param name - Property name
     *
     * @return String with formatted value of property
     */
    std::string getValue(const PropertyName& name) const
    {
        std::string ret(8, '\0');
        auto it = this->find(name);
        if (it == this->end())
        {
            ret = "N/A";
        }
        else if (std::holds_alternative<double>(it->second))
        {
            auto value = std::get<double>(it->second);
            if (std::isnan(value))
            {
                ret = "N/A";
            }
            else if (value < 1000)
            {
                snprintf(ret.data(), ret.size(), "%7.03f", value);
            }
            else
            {
                snprintf(ret.data(), ret.size(), "%7d", (int)(value));
            }
        }
        else
        {
            auto factor = scale();
            auto value = std::get<int64_t>(it->second);
            if (factor < 1.f)
            {
                snprintf(ret.data(), ret.size(), "%7.03f", value * factor);
            }
            else
            {
                snprintf(ret.data(), ret.size(), "%7d", (int)(value * factor));
            }
        }

        return ret;
    }
};

/**
 * @brief Show sensor's data
 *
 * @param service - Sensor's object service
 * @param path - Sensor's object path
 */
void printSensorData(const std::string& service, const std::string& path)
{
    // Ask DBus for all sensors properties
    auto m = systemBus.new_method_call(service.c_str(), path.c_str(),
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
    constexpr auto row_fmt = "%-18s %8s %7s %-4s %7s %7s %7s %7s %7s\n";

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
               "UC", "NR");
        printf("\n");

        typeName = currentType;
    }

    // Show sensor data
    printf(row_fmt, path.c_str() + name_pos + 1, props.status().c_str(),
           props.value().c_str(), props.unit().c_str(),
           props.criticalLow().c_str(), props.warningLow().c_str(),
           props.warningHigh().c_str(), props.criticalHigh().c_str(),
           props.fatalHigh().c_str());
}

/**
 * @brief compare sensors path with numbers
 */
struct CmpSensorsName
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        const char* strA = a.c_str();
        const char* strB = b.c_str();
        while (true)
        {
            const char& chrA = *strA;
            const char& chrB = *strB;

            // check for end of name
            if (!chrA || !chrB)
            {
                return !!chrB;
            }

            const bool isNumA = (chrA >= '0' && chrA <= '9');
            const bool isNumB = (chrB >= '0' && chrB <= '9');

            if (isNumA && isNumB)
            {
                // both names have numbers at the same position
                char* endA = nullptr;
                char* endB = nullptr;
                const unsigned long valA = strtoul(strA, &endA, 10);
                const unsigned long valB = strtoul(strB, &endB, 10);

                if (valA != valB)
                {
                    return valA < valB;
                }

                strA = endA;
                strB = endB;
            }
            else if (isNumA || isNumB)
            {
                // only one of names has a number
                return isNumA;
            }
            else
            {
                // no digits at position
                if (chrA != chrB)
                {
                    return chrA < chrB;
                }

                ++strA;
                ++strB;
            }
        }
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

    auto method = systemBus.new_method_call(MAPPER_SERVICE, MAPPER_PATH,
                                            MAPPER_IFACE, "GetSubTree");
    const std::vector<std::string> ifaces = {SENSOR_VALUE_IFACE};
    method.append(root_path, 0, ifaces);

    using Path = std::string;
    using Service = std::string;
    using Interface = std::string;
    using Interfaces = std::vector<Interface>;
    using ObjectsMap = std::map<Service, Interfaces>;
    using Objects = std::map<Path, ObjectsMap, CmpSensorsName>;

    Objects objects;
    try
    {
        systemBus.call(method).read(objects);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        fprintf(stderr, "Error: %s\n", ex.what());
        return EXIT_FAILURE;
    }

    for (const auto& obj : objects)
    {
        for (const auto& service : obj.second)
        {
            printSensorData(service.first, obj.first);
        }
    }

    return EXIT_SUCCESS;
}
