#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <map>

class Config {
private:
    std::map<std::string, std::string> values;
    std::string configPath;

public:
    Config(const std::string& path = "cs2_tool.ini") : configPath(path) {
        Load();
    }

    void Load() {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            // Create default config
            SetDefaults();
            Save();
            return;
        }

        values.clear();
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;

            // Parse key=value
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                values[key] = value;
            }
        }
        file.close();
    }

    void Save() {
        std::ofstream file(configPath);
        if (!file.is_open()) return;

        file << "; CS2 External Tool Configuration\n";
        file << "; Generated automatically - edit values as needed\n\n";

        for (const auto& pair : values) {
            file << pair.first << " = " << pair.second << "\n";
        }

        file.close();
    }

    void SetDefaults() {
        values.clear();
        
        // ESP Settings
        values["esp.enabled"] = "true";
        values["esp.boxes"] = "true";
        values["esp.health"] = "true";
        values["esp.name"] = "true";
        values["esp.weapon"] = "true";
        values["esp.distance"] = "true";
        values["esp.snaplines"] = "false";
        values["esp.skeleton"] = "false";
        
        // Aimbot Settings
        values["aimbot.enabled"] = "false";
        values["aimbot.smoothing"] = "0.15";
        values["aimbot.fov"] = "15.0";
        values["aimbot.target"] = "head"; // head or chest
        
        // Hotkeys
        values["hotkey.aim"] = "VK_XBUTTON1"; // VK_LBUTTON, VK_XBUTTON1, VK_RBUTTON
        values["hotkey.menu"] = "VK_INSERT";
        values["hotkey.unload"] = "VK_END";
        
        // Colors
        values["color.enemy"] = "255,0,0,255"; // RGBA
        values["color.teammate"] = "0,255,0,255";
        values["color.visible"] = "255,255,0,255";
        
        // Misc
        values["overlay.refresh_rate"] = "144";
        values["overlay.width"] = "1920";
        values["overlay.height"] = "1080";
    }

    bool GetBool(const std::string& key, bool defaultValue = false) {
        auto it = values.find(key);
        if (it == values.end()) return defaultValue;
        
        std::string value = it->second;
        for (auto& c : value) c = tolower(c);
        
        return value == "true" || value == "1" || value == "yes";
    }

    int GetInt(const std::string& key, int defaultValue = 0) {
        auto it = values.find(key);
        if (it == values.end()) return defaultValue;
        
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    float GetFloat(const std::string& key, float defaultValue = 0.0f) {
        auto it = values.find(key);
        if (it == values.end()) return defaultValue;
        
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    std::string GetString(const std::string& key, const std::string& defaultValue = "") {
        auto it = values.find(key);
        if (it == values.end()) return defaultValue;
        return it->second;
    }

    void SetBool(const std::string& key, bool value) {
        values[key] = value ? "true" : "false";
    }

    void SetInt(const std::string& key, int value) {
        values[key] = std::to_string(value);
    }

    void SetFloat(const std::string& key, float value) {
        values[key] = std::to_string(value);
    }

    void SetString(const std::string& key, const std::string& value) {
        values[key] = value;
    }

    // Virtual key code conversion
    int GetVirtualKey(const std::string& key, int defaultValue = 0) {
        std::string vkName = GetString(key);
        if (vkName.empty()) return defaultValue;

        if (vkName == "VK_LBUTTON") return VK_LBUTTON;
        if (vkName == "VK_RBUTTON") return VK_RBUTTON;
        if (vkName == "VK_XBUTTON1") return VK_XBUTTON1;
        if (vkName == "VK_XBUTTON2") return VK_XBUTTON2;
        if (vkName == "VK_INSERT") return VK_INSERT;
        if (vkName == "VK_END") return VK_END;
        if (vkName == "VK_HOME") return VK_HOME;
        if (vkName == "VK_DELETE") return VK_DELETE;
        if (vkName == "VK_SHIFT") return VK_SHIFT;
        if (vkName == "VK_CONTROL") return VK_CONTROL;
        if (vkName == "VK_MENU") return VK_MENU; // ALT
        
        // Try to parse as hex or decimal
        try {
            if (vkName.substr(0, 2) == "0x") {
                return std::stoi(vkName, nullptr, 16);
            }
            return std::stoi(vkName);
        } catch (...) {
            return defaultValue;
        }
    }
};
