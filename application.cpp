#include <sdbus-c++/sdbus-c++.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>
#include "json/json.hpp"

using nlohmann::json;

int global_timeout = 0;
std::string global_timeout_phrase = "";

std::mutex params_mutex;

void onConfigChanged(sdbus::Signal signal)
{
    std::map<std::string, sdbus::Variant> newConfig;
    signal >> newConfig;

    auto it = newConfig.find("TimeoutPhrase");
    auto it2 = newConfig.find("Timeout");
    //  Lock here
    const std::lock_guard<std::mutex> lock(params_mutex);
    if (it != newConfig.end() && it->second.containsValueOfType<std::string>()) {
        std::string timeout_phrase = it->second.get<std::string>();
        std::cout << "onConfigChanged: Received timeout phrase: " << timeout_phrase << std::endl;
        global_timeout_phrase = timeout_phrase;
    }
    if (it2 != newConfig.end() && it2->second.containsValueOfType<int32_t>()) {
        int timeout = it2->second.get<int>();
        std::cout << "onConfigChanged: Received timeout: " << timeout << std::endl;
        global_timeout = timeout;
    }
    // Unlock here
}

int main(int argc, char *argv[])
{
    sdbus::ServiceName destination{ "com.system.configurationManager" };
    sdbus::ObjectPath objectPath{ "/com/system/configurationManager/Application/confManagerApplication1" };

    // Create proxy object for the configurator object on the server side, on the session bus
    auto connection = sdbus::createSessionBusConnection();
    auto configProxy = sdbus::createProxy(std::move(connection), std::move(destination), std::move(objectPath));

    // Let's subscribe for the 'configChanged' signals
    sdbus::InterfaceName interfaceName{"com.system.configurationManager.Application.Configuration"};
    sdbus::SignalName signalName{"configurationChanged"};
    configProxy->registerSignalHandler(interfaceName, signalName, &onConfigChanged);

    // Read configuration information
    std::string home_dir = getenv("HOME");
    std::string file_name = home_dir + "/com.system.configurationManager/confManagerApplication1.json";
    std::ifstream ifs(file_name);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open configuration file: " << file_name << std::endl;
        return 1;
    }
    json in_json;
    try {
        int timeout;
        std::string timeout_phrase;
        ifs >> in_json;
        timeout = in_json.value("Timeout", 0);
        timeout_phrase = in_json.value("TimeoutPhrase", "");
        if (timeout == 0 || timeout_phrase == "") {
            std::cerr << "Incorrect configuration in the file!\n";
            return 1;
        }
        global_timeout = timeout;
        global_timeout_phrase = timeout_phrase;
    } catch (const std::exception &e) {
        std::cerr << "Exception parsing configuration file: " << e.what() << std::endl;
        return 1;
    }

    for (;;) {
        int timeout;
        {
            // Lock here
            const std::lock_guard<std::mutex> lock(params_mutex);
        
            std::cout << "\"" << global_timeout_phrase << "\"\n";
            timeout = global_timeout;
            // Unlock here
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    return 0;
}
