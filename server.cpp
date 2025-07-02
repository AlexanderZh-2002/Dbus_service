#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>
#include <map>
#include <algorithm>
#include "json/json.hpp"

using nlohmann::json;

// Define the D-Bus interface, object path, and service name
const std::string SERVICE_NAME = "com.system.configurationManager";
const std::string OBJECT_PATH = "/org/example/DemoService";
//const std::string INTERFACE_NAME = "org.example.DemoInterface";

sdbus::ObjectPath SDBUS_OBJECT_PATH{ OBJECT_PATH };

const std::string CONFIG_OBJECT_PATH_PREFIX = "/com/system/configurationManager/Application/";
const std::string CONFIG_INTERFACE_NAME = "com.system.configurationManager.Application.Configuration";

typedef std::map<std::string, sdbus::Variant> SdbusDict;


class ConfigService;

std::map<std::string, std::shared_ptr<ConfigService>> configServiceMap;

class ConfigService {
    public:
        explicit ConfigService(sdbus::IConnection& connection, const sdbus::ObjectPath &path, const json &in_json);
        bool changeConfiguration(const std::string &key, const sdbus::Variant &value);
        SdbusDict &getConfiguration() { 
            return m_configuration; 
        };
        void emitConfigurationChanged();
        ~ConfigService() { std::cerr << "Destroying object: " << m_path << std::endl; }
    private:
        void fillFromJson(const json& in_json);
        void UpdateToPersistent() {};
        bool GetFromPersistent(const json &in_json) { return true; };
    private:
        std::unique_ptr<sdbus::IObject> m_object;
        std::string m_path;
        SdbusDict m_configuration;
};



void change_configuration_callback(sdbus::MethodCall call)
{
    std::cout << "change_config_callback\n";

    // Deserialize the key from the message
    std::string key;
    call >> key;
    
    std::cout << "1\n";

    // Deserialize the value from the message
    sdbus::Variant value;
    call >> value;

    std::cout << "2\n";

    std::cout << "change configuration on:" << call.getPath() << std::endl;

    // Return error if there is no configuration object for the path
    std::string objectPath = call.getPath();
    auto it = configServiceMap.find(objectPath);
    if (it == configServiceMap.end()) {
        throw sdbus::Error(sdbus::Error::Name{ "org.sdbuscpp.Configurator.Error" }, "Application not found: " + objectPath);
    }

    // Return error if change operation fails
    auto configObject = it->second;
    bool result = configObject->changeConfiguration(key, value);
    if (!result) {
        throw sdbus::Error(sdbus::Error::Name{ "org.sdbuscpp.Configurator.Error" }, "Failed to update property: " + key + " for application: " + objectPath);
    }
    
    configObject->emitConfigurationChanged();
    
    auto reply = call.createReply();
    //reply << key;
    reply.send();
}

void get_configuration_callback(sdbus::MethodCall call)
{
    std::cout << "get configuration on:" << call.getPath() << std::endl;

    // Return error if there is no configuration object for the path
    std::string objectPath = call.getPath();
    auto it = configServiceMap.find(objectPath);
    if (it == configServiceMap.end()) {
        throw sdbus::Error(sdbus::Error::Name{ "org.sdbuscpp.Configurator.Error" }, "Application not found: " + objectPath);
    }

    // Return error if change operation fails
    auto configObject = it->second;
    auto result = configObject->getConfiguration();

    // Serialize resulting map to the reply and send the reply to the caller
    auto reply = call.createReply();
    reply << result;
    reply.send();
}

ConfigService::ConfigService(sdbus::IConnection& connection, const sdbus::ObjectPath &path, const json &in_json)
    : m_object(sdbus::createObject(connection, path)), m_path(path)
{
    // Register the D-Bus interface
    sdbus::InterfaceName interfaceName{ CONFIG_INTERFACE_NAME };
    m_object->addVTable(
        sdbus::MethodVTableItem{ sdbus::MethodName{"ChangeConfiguration"}, sdbus::Signature{"sv"}, {}, sdbus::Signature{""}, {}, &change_configuration_callback, {} },
        sdbus::MethodVTableItem{ sdbus::MethodName{"GetConfiguration"}, sdbus::Signature{""}, {}, sdbus::Signature{"a{sv}"}, {}, &get_configuration_callback, {} },
        sdbus::SignalVTableItem{ sdbus::MethodName{"configurationChanged"}, sdbus::Signature{"a{sv}"}, {}, {} })
        .forInterface(interfaceName);

    std::cout << "Registered on path: " << path << std::endl;
    //  Fill configuration from JSON
    fillFromJson(in_json);
 }

void ConfigService::fillFromJson(const json& in_json)
{
    for (auto it = in_json.begin(); it != in_json.end(); it++) {
        std::string key = it.key();
        const json& jvalue = it.value();
        sdbus::Variant value;
        if (jvalue.is_number_integer()) {
            long actual = jvalue;
            value = sdbus::Variant(actual);
        } else if (jvalue.is_string()) {
            std::string actual = jvalue;
            value = sdbus::Variant(actual);
        } else if (jvalue.is_number_float()) {
            double actual = jvalue;
            value = sdbus::Variant(actual);
        }
        m_configuration[key] = value;
    }
}

bool ConfigService::changeConfiguration(const std::string &key, const sdbus::Variant &value) {
    auto it = m_configuration.find(key);
    if (it == m_configuration.end()) {
        return false;
    }
    it->second = value;
    UpdateToPersistent();
    return true;
};

void ConfigService::emitConfigurationChanged()
{
    auto signal = m_object->createSignal(sdbus::InterfaceName(CONFIG_INTERFACE_NAME), sdbus::SignalName("configurationChanged"));
    signal << m_configuration;
    m_object->emitSignal(signal);
    std::cout << "Emitted configuration changed signal for path: " << m_path << std::endl;
}


int createConfigurationObjects(sdbus::IConnection &connection)
{
    std::string home_dir = getenv("HOME");
    std::filesystem::path directory_path = home_dir + "/com.system.configurationManager"; 
    int valid_count = 0;

    // Use a range-based for loop to iterate over directory entries
    for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
        std::cout << entry.path() << std::endl; // Print each entry
        std::string app_name = entry.path().stem();
        std::replace( app_name.begin(), app_name.end(), '.', '_');
        std::ifstream ifs(entry.path());
        if (ifs.is_open()) {
            json in_json;
            try {
                ifs >> in_json;
                for (auto it = in_json.begin(); it != in_json.end(); it++) {
                    std::cout << "\"" << it.key() << "\"\n";
                }
                sdbus::ObjectPath objectPath = sdbus::ObjectPath{ CONFIG_OBJECT_PATH_PREFIX + app_name };
                configServiceMap[objectPath] = std::make_shared<ConfigService>(connection, objectPath, in_json);
                valid_count ++;
            }
            catch(const std::exception& e) {
                std::cerr << "Error reading " << entry.path() << ": " << e.what() << std::endl;
            }
        }
    }
    return valid_count;
}   


int main(int argc, char *argv[])
{
    try {
        // Create a connection to the session bus
        sdbus::ServiceName serviceName{ SERVICE_NAME };
        auto connection = sdbus::createSessionBusConnection(serviceName);

        std::cout << "After create connection\n";

        // Create configuration objects for the config directory
        int valid_count = createConfigurationObjects(*connection);
        std::cout << "Found " << valid_count << " valid configuration files" << std::endl;
        if (valid_count == 0) {
            return 0;
        }


        std::cout << "After creating service object\n";

        // Enter the event loop to handle incoming D-Bus calls
        std::cout << "D-Bus service running. Waiting for calls..." << std::endl;
        connection->enterEventLoop();
    }
    catch (const sdbus::Error& e) {
        std::cerr << "sdbus error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;

 }
