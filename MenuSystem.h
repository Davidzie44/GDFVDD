#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "PrimitivesRenderer.h"
#include "FontRenderer.h"
#include "Common.h"

enum class MenuItemType {
    TOGGLE,
    SLIDER,
    COLOR,
    DROPDOWN
};

struct MenuItem {
    MenuItemType type;
    std::string label;
    std::string hotkey;
    
    // Toggle
    bool* boolValue;
    
    // Slider
    float* floatValue;
    float minVal;
    float maxVal;
    float step;
    
    // Color
    D3DXCOLOR* colorValue;
    
    // Dropdown
    int* selectedIndex;
    std::vector<std::string> options;
};

struct MenuTab {
    std::string name;
    std::vector<MenuItem> items;
};

class CMenu {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    
    bool isOpen;
    int currentTab;
    int selectedItem;
    
    std::vector<MenuTab> tabs;
    
    float menuX;
    float menuY;
    float menuWidth;
    float menuHeight;
    
    // Colors
    D3DXCOLOR backgroundColor;
    D3DXCOLOR borderColor;
    D3DXCOLOR textColor;
    D3DXCOLOR accentColor;
    D3DXCOLOR highlightColor;
    
    // Key states
    bool upPressed;
    bool downPressed;
    bool leftPressed;
    bool rightPressed;
    bool enterPressed;
    bool tabPressed;
    
    void DrawTab(float x, float y, float width, const std::string& name, bool isActive) {
        float tabHeight = 30.0f;
        
        if (isActive) {
            primitives->DrawFilledRect(x, y, width, tabHeight, accentColor);
            renderer->DrawText(x + 10, y + 8, name.c_str(), D3DXCOLOR(1, 1, 1, 1), false, renderer->GetMainFont());
        } else {
            primitives->DrawFilledRect(x, y, width, tabHeight, backgroundColor);
            primitives->DrawRect(x, y, width, tabHeight, borderColor, 1.0f);
            renderer->DrawText(x + 10, y + 8, name.c_str(), textColor, false, renderer->GetMainFont());
        }
    }
    
    void DrawToggleItem(float x, float y, const MenuItem& item, bool isSelected) {
        float itemHeight = 30.0f;
        
        if (isSelected) {
            primitives->DrawFilledRect(x, y, menuWidth - 20, itemHeight, highlightColor);
        }
        
        // Draw checkbox
        float checkboxX = x + 10;
        float checkboxY = y + 5;
        float checkboxSize = 20.0f;
        
        primitives->DrawRect(checkboxX, checkboxY, checkboxSize, checkboxSize, borderColor, 1.0f);
        
        if (*item.boolValue) {
            primitives->DrawFilledRect(checkboxX + 2, checkboxY + 2, checkboxSize - 4, checkboxSize - 4, accentColor);
        }
        
        // Draw label
        renderer->DrawText(checkboxX + checkboxSize + 10, y + 8, item.label.c_str(), textColor, false, renderer->GetMainFont());
        
        // Draw hotkey
        if (!item.hotkey.empty()) {
            renderer->DrawText(x + menuWidth - 80, y + 8, item.hotkey.c_str(), D3DXCOLOR(0.5f, 0.5f, 0.5f, 1), false, renderer->GetSmallFont());
        }
    }
    
    void DrawSliderItem(float x, float y, const MenuItem& item, bool isSelected) {
        float itemHeight = 40.0f;
        
        if (isSelected) {
            primitives->DrawFilledRect(x, y, menuWidth - 20, itemHeight, highlightColor);
        }
        
        // Draw label
        renderer->DrawText(x + 10, y + 5, item.label.c_str(), textColor, false, renderer->GetMainFont());
        
        // Draw slider bar
        float sliderX = x + 10;
        float sliderY = y + 25;
        float sliderWidth = menuWidth - 100;
        float sliderHeight = 8.0f;
        
        primitives->DrawRect(sliderX, sliderY, sliderWidth, sliderHeight, borderColor, 1.0f);
        
        // Calculate fill
        float percentage = (*item.floatValue - item.minVal) / (item.maxVal - item.minVal);
        float fillWidth = sliderWidth * percentage;
        
        primitives->DrawFilledRect(sliderX, sliderY, fillWidth, sliderHeight, accentColor);
        
        // Draw value
        char valueStr[32];
        sprintf_s(valueStr, "%.2f", *item.floatValue);
        renderer->DrawText(x + menuWidth - 80, y + 5, valueStr, textColor, false, renderer->GetMainFont());
    }
    
    void DrawDropdownItem(float x, float y, const MenuItem& item, bool isSelected) {
        float itemHeight = 30.0f;
        
        if (isSelected) {
            primitives->DrawFilledRect(x, y, menuWidth - 20, itemHeight, highlightColor);
        }
        
        // Draw label
        renderer->DrawText(x + 10, y + 8, item.label.c_str(), textColor, false, renderer->GetMainFont());
        
        // Draw current option
        if (*item.selectedIndex >= 0 && *item.selectedIndex < (int)item.options.size()) {
            renderer->DrawText(x + 150, y + 8, item.options[*item.selectedIndex].c_str(), accentColor, false, renderer->GetMainFont());
        }
        
        // Draw arrow
        primitives->DrawLine(x + menuWidth - 30, y + 15, x + menuWidth - 20, y + 15, textColor, 2.0f);
    }

public:
    CMenu() : primitives(nullptr), renderer(nullptr), isOpen(false), currentTab(0),
              selectedItem(0), menuX(50), menuY(50), menuWidth(400), menuHeight(500),
              upPressed(false), downPressed(false),
              leftPressed(false), rightPressed(false), enterPressed(false), tabPressed(false) {
        backgroundColor = D3DXCOLOR(20.0f/255.0f, 20.0f/255.0f, 25.0f/255.0f, 200.0f/255.0f);
        borderColor = D3DXCOLOR(1, 1, 1, 1);
        textColor = D3DXCOLOR(1, 1, 1, 1);
        accentColor = D3DXCOLOR(0, 170.0f/255.0f, 1, 1);
        highlightColor = D3DXCOLOR(0, 170.0f/255.0f, 1, 30.0f/255.0f);
    }
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend) {
        primitives = prim;
        renderer = rend;
    }
    
    void AddTab(const std::string& name) {
        MenuTab tab;
        tab.name = name;
        tabs.push_back(tab);
    }
    
    void AddToggleItem(const std::string& tabName, const std::string& label, bool* value, const std::string& hotkey = "") {
        for (auto& tab : tabs) {
            if (tab.name == tabName) {
                MenuItem item;
                item.type = MenuItemType::TOGGLE;
                item.label = label;
                item.hotkey = hotkey;
                item.boolValue = value;
                tab.items.push_back(item);
                break;
            }
        }
    }
    
    void AddSliderItem(const std::string& tabName, const std::string& label, float* value, float min, float max, float step) {
        for (auto& tab : tabs) {
            if (tab.name == tabName) {
                MenuItem item;
                item.type = MenuItemType::SLIDER;
                item.label = label;
                item.floatValue = value;
                item.minVal = min;
                item.maxVal = max;
                item.step = step;
                tab.items.push_back(item);
                break;
            }
        }
    }
    
    void AddDropdownItem(const std::string& tabName, const std::string& label, int* value, const std::vector<std::string>& options) {
        for (auto& tab : tabs) {
            if (tab.name == tabName) {
                MenuItem item;
                item.type = MenuItemType::DROPDOWN;
                item.label = label;
                item.selectedIndex = value;
                item.options = options;
                tab.items.push_back(item);
                break;
            }
        }
    }
    
    void HandleInput() {
        if (!isOpen) return;
        
        // UP - Move selection up
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            if (!upPressed) {
                if (selectedItem > 0) selectedItem--;
                upPressed = true;
            }
        } else {
            upPressed = false;
        }
        
        // DOWN - Move selection down
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            if (!downPressed) {
                if (currentTab < (int)tabs.size() && selectedItem < (int)tabs[currentTab].items.size() - 1) {
                    selectedItem++;
                }
                downPressed = true;
            }
        } else {
            downPressed = false;
        }
        
        // LEFT - Decrease slider value
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
            if (!leftPressed) {
                if (currentTab < (int)tabs.size() && selectedItem < (int)tabs[currentTab].items.size()) {
                    MenuItem& item = tabs[currentTab].items[selectedItem];
                    if (item.type == MenuItemType::SLIDER) {
                        *item.floatValue -= item.step;
                        if (*item.floatValue < item.minVal) *item.floatValue = item.minVal;
                    } else if (item.type == MenuItemType::DROPDOWN) {
                        if (*item.selectedIndex > 0) (*item.selectedIndex)--;
                    }
                }
                leftPressed = true;
            }
        } else {
            leftPressed = false;
        }
        
        // RIGHT - Increase slider value
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            if (!rightPressed) {
                if (currentTab < (int)tabs.size() && selectedItem < (int)tabs[currentTab].items.size()) {
                    MenuItem& item = tabs[currentTab].items[selectedItem];
                    if (item.type == MenuItemType::SLIDER) {
                        *item.floatValue += item.step;
                        if (*item.floatValue > item.maxVal) *item.floatValue = item.maxVal;
                    } else if (item.type == MenuItemType::DROPDOWN) {
                        if (*item.selectedIndex < (int)item.options.size() - 1) (*item.selectedIndex)++;
                    }
                }
                rightPressed = true;
            }
        } else {
            rightPressed = false;
        }
        
        // ENTER - Toggle checkbox
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
            if (!enterPressed) {
                if (currentTab < (int)tabs.size() && selectedItem < (int)tabs[currentTab].items.size()) {
                    MenuItem& item = tabs[currentTab].items[selectedItem];
                    if (item.type == MenuItemType::TOGGLE) {
                        *item.boolValue = !*item.boolValue;
                    }
                }
                enterPressed = true;
            }
        } else {
            enterPressed = false;
        }
        
        // TAB - Switch tabs
        if (GetAsyncKeyState(VK_TAB) & 0x8000) {
            if (!tabPressed) {
                currentTab = (currentTab + 1) % tabs.size();
                selectedItem = 0;
                tabPressed = true;
            }
        } else {
            tabPressed = false;
        }
    }
    
    void Draw(float x, float y) {
        if (!isOpen) return;
        
        menuX = x;
        menuY = y;
        
        // Calculate menu height based on content
        menuHeight = 60; // Header + tabs
        for (const auto& tab : tabs) {
            for (const auto& item : tab.items) {
                menuHeight += (item.type == MenuItemType::SLIDER) ? 40 : 30;
            }
        }
        
        // Draw background
        primitives->DrawFilledRect(menuX, menuY, menuWidth, menuHeight, backgroundColor);
        primitives->DrawRect(menuX, menuY, menuWidth, menuHeight, borderColor, 1.0f);
        
        // Draw tabs
        float tabWidth = menuWidth / tabs.size();
        float tabY = menuY + 30;
        
        for (size_t i = 0; i < tabs.size(); i++) {
            DrawTab(menuX + i * tabWidth, tabY, tabWidth, tabs[i].name, i == currentTab);
        }
        
        // Draw separator
        primitives->DrawLine(menuX, tabY + 30, menuX + menuWidth, tabY + 30, borderColor, 1.0f);
        
        // Draw items for current tab
        float itemY = tabY + 35;
        if (currentTab < (int)tabs.size()) {
            for (size_t i = 0; i < tabs[currentTab].items.size(); i++) {
                bool isSelected = (i == selectedItem);
                const MenuItem& item = tabs[currentTab].items[i];
                
                switch (item.type) {
                    case MenuItemType::TOGGLE:
                        DrawToggleItem(menuX + 10, itemY, item, isSelected);
                        itemY += 30;
                        break;
                    case MenuItemType::SLIDER:
                        DrawSliderItem(menuX + 10, itemY, item, isSelected);
                        itemY += 40;
                        break;
                    case MenuItemType::DROPDOWN:
                        DrawDropdownItem(menuX + 10, itemY, item, isSelected);
                        itemY += 30;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    void WriteConfig(const char* path) {
        std::ofstream file(path);
        if (!file.is_open()) return;
        
        file << "; CS2 Tool Configuration\n\n";
        
        for (const auto& tab : tabs) {
            file << "[" << tab.name << "]\n";
            
            for (const auto& item : tab.items) {
                switch (item.type) {
                    case MenuItemType::TOGGLE:
                        file << item.label << " = " << (*item.boolValue ? "true" : "false") << "\n";
                        break;
                    case MenuItemType::SLIDER:
                        file << item.label << " = " << *item.floatValue << "\n";
                        break;
                    case MenuItemType::DROPDOWN:
                        file << item.label << " = " << *item.selectedIndex << "\n";
                        break;
                    default:
                        break;
                }
            }
            
            file << "\n";
        }
        
        file.close();
    }
    
    void ReadConfig(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) return;
        
        std::string line;
        std::string currentSection;
        
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == ';') continue;
            
            // Section header
            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }
            
            // Key = Value
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Find matching item
                for (auto& tab : tabs) {
                    if (tab.name == currentSection) {
                        for (auto& item : tab.items) {
                            if (item.label == key) {
                                switch (item.type) {
                                    case MenuItemType::TOGGLE:
                                        *item.boolValue = (value == "true" || value == "1");
                                        break;
                                    case MenuItemType::SLIDER:
                                        *item.floatValue = std::stof(value);
                                        break;
                                    case MenuItemType::DROPDOWN:
                                        *item.selectedIndex = std::stoi(value);
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        file.close();
    }
    
    bool IsOpen() const {
        return isOpen;
    }
    
    void SetOpen(bool open) {
        isOpen = open;
    }
};
