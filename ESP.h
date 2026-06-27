#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "Overlay.h"
#include <string>
#include <sstream>
#include <iomanip>

class ESP {
private:
    ProcessMemory& process;
    EntityList& entityList;
    WorldToScreen& worldToScreen;
    Overlay& overlay;
    
    int localTeam;
    
    // Weapon name mapping (simplified - expand as needed)
    std::string GetWeaponName(uintptr_t weaponAddress) {
        if (weaponAddress == 0) return "Unknown";
        
        try {
            // Read weapon ID or name from weapon structure
            // This is a placeholder - actual offset depends on CS2 version
            int weaponId = process.ReadMemory<int>(weaponAddress + 0x0);
            
            switch (weaponId) {
                case 1: return "AK-47";
                case 2: return "M4A4";
                case 3: return "AWP";
                case 4: return "Desert Eagle";
                case 5: return "Glock-18";
                case 6: return "USP-S";
                case 7: return "P250";
                case 8: return "Five-SeveN";
                case 9: return "Tec-9";
                case 10: return "CZ75-Auto";
                case 11: return "P2000";
                case 12: return "MP7";
                case 13: return "MP9";
                case 14: return "MAC-10";
                case 15: return "UMP-45";
                case 16: return "P90";
                case 17: return "Galil AR";
                case 18: return "FAMAS";
                case 19: return "SG 553";
                case 20: return "AUG";
                case 21: return "SSG 08";
                case 22: return "SCAR-20";
                case 23: return "G3SG1";
                case 24: return "M249";
                case 25: return "Negev";
                case 26: return "Nova";
                case 27: return "XM1014";
                case 28: return "MAG-7";
                case 29: return "Sawed-Off";
                case 30: return "Bizon";
                case 31: return "PP-Bizon";
                default: return "Knife";
            }
        } catch (...) {
            return "Unknown";
        }
    }
    
    // Get player name from entity
    std::string GetPlayerName(uintptr_t entityAddress) {
        if (entityAddress == 0) return "Unknown";
        
        try {
            // Read player name from entity structure
            // This is a placeholder - actual offset depends on CS2 version
            uintptr_t namePtr = process.ReadMemory<uintptr_t>(entityAddress + 0x620);
            
            if (namePtr != 0) {
                char nameBuffer[32];
                for (int i = 0; i < 31; i++) {
                    nameBuffer[i] = process.ReadMemory<char>(namePtr + i);
                    if (nameBuffer[i] == 0) break;
                }
                nameBuffer[31] = 0;
                return std::string(nameBuffer);
            }
        } catch (...) {
            return "Unknown";
        }
        
        return "Unknown";
    }
    
    // Calculate box dimensions based on distance
    void CalculateBoxDimensions(float distance, float& boxHeight, float& boxWidth) {
        // Height scales inversely with distance
        boxHeight = 10000.0f / distance;
        boxWidth = boxHeight * 0.6f;
        
        // Clamp minimum and maximum sizes
        if (boxHeight < 20.0f) boxHeight = 20.0f;
        if (boxHeight > 300.0f) boxHeight = 300.0f;
        if (boxWidth < 12.0f) boxWidth = 12.0f;
        if (boxWidth > 180.0f) boxWidth = 180.0f;
    }
    
    // Draw health bar with gradient
    void DrawHealthBar(float x, float y, float width, float height, int health, int maxHealth = 100) {
        // Background (black)
        overlay.DrawFilledRect(x, y, width, height, Color(0, 0, 0, 0.7f));
        
        // Calculate health percentage
        float healthPercent = (float)health / (float)maxHealth;
        if (healthPercent < 0) healthPercent = 0;
        if (healthPercent > 1) healthPercent = 1;
        
        // Health bar width
        float healthWidth = width * healthPercent;
        
        // Color based on health (green -> yellow -> red)
        Color healthColor;
        if (healthPercent > 0.5f) {
            // Green to yellow
            healthColor = Color(2.0f * (1.0f - healthPercent), 1.0f, 0, 1);
        } else {
            // Yellow to red
            healthColor = Color(1.0f, 2.0f * healthPercent, 0, 1);
        }
        
        // Draw health bar
        overlay.DrawFilledRect(x, y, healthWidth, height, healthColor);
        
        // Draw border
        overlay.DrawRect(x, y, width, height, Color(1, 1, 1, 0.5f));
    }
    
    // Draw ESP box for an entity
    void DrawESPBox(const Entity& entity, const Vector3& screenPos, float distance) {
        int health = entity.GetHealth();
        int team = entity.GetTeam();
        
        // Calculate box dimensions
        float boxHeight, boxWidth;
        CalculateBoxDimensions(distance, boxHeight, boxWidth);
        
        // Box position (centered on screen position)
        float boxX = screenPos.x - boxWidth / 2.0f;
        float boxY = screenPos.y - boxHeight / 2.0f;
        
        // Determine color based on team
        Color boxColor;
        if (team == localTeam) {
            // Teammate - green
            boxColor = Color(0, 1, 0, 1);
        } else {
            // Enemy - red
            boxColor = Color(1, 0, 0, 1);
        }
        
        // Draw box outline
        overlay.DrawRect(boxX, boxY, boxWidth, boxHeight, boxColor);
        
        // Draw corners (thicker)
        float cornerSize = boxWidth * 0.2f;
        float cornerThickness = 2.0f;
        
        // Top-left corner
        overlay.DrawLine(boxX, boxY, boxX + cornerSize, boxY, boxColor, cornerThickness);
        overlay.DrawLine(boxX, boxY, boxX, boxY + cornerSize, boxColor, cornerThickness);
        
        // Top-right corner
        overlay.DrawLine(boxX + boxWidth - cornerSize, boxY, boxX + boxWidth, boxY, boxColor, cornerThickness);
        overlay.DrawLine(boxX + boxWidth, boxY, boxX + boxWidth, boxY + cornerSize, boxColor, cornerThickness);
        
        // Bottom-left corner
        overlay.DrawLine(boxX, boxY + boxHeight - cornerSize, boxX, boxY + boxHeight, boxColor, cornerThickness);
        overlay.DrawLine(boxX, boxY + boxHeight, boxX + cornerSize, boxY + boxHeight, boxColor, cornerThickness);
        
        // Bottom-right corner
        overlay.DrawLine(boxX + boxWidth - cornerSize, boxY + boxHeight, boxX + boxWidth, boxY + boxHeight, boxColor, cornerThickness);
        overlay.DrawLine(boxX + boxWidth, boxY + boxHeight - cornerSize, boxX + boxWidth, boxY + boxHeight, boxColor, cornerThickness);
        
        // Draw health bar above box
        float healthBarWidth = boxWidth;
        float healthBarHeight = 4.0f;
        float healthBarX = boxX;
        float healthBarY = boxY - healthBarHeight - 2.0f;
        DrawHealthBar(healthBarX, healthBarY, healthBarWidth, healthBarHeight, health);
        
        // Draw player name below box
        std::string playerName = GetPlayerName(entity.GetAddress());
        float textY = boxY + boxHeight + 2.0f;
        overlay.DrawText(boxX, textY, playerName, Color(1, 1, 1, 1), 14.0f);
        
        // Draw weapon name below player name
        uintptr_t weaponAddress = 0; // TODO: Get weapon address from entity
        std::string weaponName = GetWeaponName(weaponAddress);
        overlay.DrawText(boxX, textY + 16.0f, weaponName, Color(1, 1, 1, 0.8f), 12.0f);
        
        // Draw distance
        std::ostringstream distanceStr;
        distanceStr << std::fixed << std::setprecision(0) << distance << "m";
        overlay.DrawText(boxX + boxWidth + 5.0f, boxY, distanceStr.str(), Color(1, 1, 1, 0.7f), 12.0f);
    }
    
    // Draw skeleton (lines connecting body parts)
    void DrawSkeleton(const Entity& entity) {
        // Placeholder for skeleton drawing
        // Would require bone position offsets and multiple WorldToScreen calls
    }
    
    // Draw snaplines (line from screen center to entity)
    void DrawSnapline(const Vector3& screenPos, const Color& color) {
        float centerX = overlay.GetWidth() / 2.0f;
        float centerY = overlay.GetHeight() / 2.0f;
        
        overlay.DrawLine(centerX, centerY, screenPos.x, screenPos.y, color, 1.0f);
    }

public:
    ESP(ProcessMemory& pm, EntityList& el, WorldToScreen& wts, Overlay& ov)
        : process(pm), entityList(el), worldToScreen(wts), overlay(ov), localTeam(0) {}
    
    void Update() {
        // Update world-to-screen matrix
        worldToScreen.Update();
        
        // Get local player team
        uintptr_t localPlayerPawn = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerPawn
        );
        
        if (localPlayerPawn != 0) {
            Entity localPlayer(process, localPlayerPawn);
            localTeam = localPlayer.GetTeam();
        }
    }
    
    void Render() {
        // Get all entities
        auto entities = entityList.GetAllEntities(64);
        
        // Get camera position for distance calculation
        Vector3 cameraPos = worldToScreen.GetCameraPosition();
        
        for (const auto& entity : entities) {
            // Read entity data
            int health = entity.GetHealth();
            int team = entity.GetTeam();
            bool dormant = entity.IsDormant();
            Vector3 position = entity.GetPosition();
            
            // Filter entities
            if (health <= 0) continue;
            if (dormant) continue;
            if (team == localTeam) continue; // Skip teammates
            
            // Calculate distance
            float distance = cameraPos.Distance(position);
            if (distance < 1.0f || distance > 1000.0f) continue; // Skip too close or too far
            
            // Project to screen
            Vector3 screenPos;
            if (!worldToScreen.WorldToScreenPoint(position, screenPos)) continue;
            
            // Determine color based on team
            Color espColor;
            if (team == localTeam) {
                espColor = Color(0, 1, 0, 1); // Green for teammates
            } else {
                espColor = Color(1, 0, 0, 1); // Red for enemies
            }
            
            // Draw ESP
            DrawESPBox(entity, screenPos, distance);
            
            // Optional: Draw snapline
            // DrawSnapline(screenPos, espColor);
            
            // Optional: Draw skeleton
            // DrawSkeleton(entity);
        }
    }
    
    void SetLocalTeam(int team) {
        localTeam = team;
    }
    
    int GetLocalTeam() const {
        return localTeam;
    }
};
