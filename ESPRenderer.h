#pragma once

#include "PrimitivesRenderer.h"
#include "FontRenderer.h"
#include "EntityManager.h"
#include "WorldToScreen.h"
#include "MenuSystem.h"
#include "Common.h"
#include <vector>

struct ESPSettings {
    bool boxes = true;
    bool healthBar = true;
    bool name = true;
    bool weapon = true;
    bool distance = true;
    bool snaplines = false;
    bool glow = false;
    bool showTeammates = false;
    bool headDot = true;
    bool fovCircle = false;
    bool offScreenIndicators = true;
    
    D3DXCOLOR enemyColor = D3DXCOLOR(1, 0, 0, 1);
    D3DXCOLOR teammateColor = D3DXCOLOR(0, 1, 0, 1);
    D3DXCOLOR visibleColor = D3DXCOLOR(1, 1, 0, 1);
    
    float aimbotFOV = 15.0f;
};

class CESPRenderer {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    WorldToScreen* worldToScreen;
    EntityManager* entityManager;
    
    ESPSettings settings;
    int screenWidth;
    int screenHeight;
    
    D3DXCOLOR GetHealthColor(float health, float maxHealth) {
        float percentage = health / maxHealth;
        if (percentage > 0.5f) {
            // Green to yellow
            float t = (percentage - 0.5f) * 2.0f;
            return D3DXCOLOR(1.0f - t, 1.0f, 0, 1);
        } else {
            // Yellow to red
            float t = percentage * 2.0f;
            return D3DXCOLOR(1.0f, t, 0, 1);
        }
    }
    
    float Max(float a, float b) { return (a > b) ? a : b; }
    float Min(float a, float b) { return (a < b) ? a : b; }
    
    std::string GetWeaponName(int weaponId) {
        // Simplified weapon name mapping
        switch (weaponId) {
            case 1: return "AK-47";
            case 2: return "M4A4";
            case 3: return "AWP";
            case 4: return "Deagle";
            case 5: return "Glock";
            case 6: return "USP";
            default: return "Unknown";
        }
    }
    
    void DrawOffScreenIndicator(const Vector3& worldPos, const D3DXCOLOR& color) {
        // Calculate angle to target
        const PlayerData& localPlayer = entityManager->GetLocalPlayer();
        Vector3 localPos = localPlayer.position;
        
        float dx = worldPos.x - localPos.x;
        float dy = worldPos.y - localPos.y;
        float angle = std::atan2(dy, dx);
        
        // Project to screen edge
        float centerX = screenWidth / 2.0f;
        float centerY = screenHeight / 2.0f;
        float radius = Min((float)screenWidth, (float)screenHeight) / 2.0f - 20.0f;
        
        float arrowX = centerX + std::cos(angle) * radius;
        float arrowY = centerY + std::sin(angle) * radius;
        
        // Clamp to screen bounds
        arrowX = Max(20.0f, Min((float)screenWidth - 20.0f, arrowX));
        arrowY = Max(20.0f, Min((float)screenHeight - 20.0f, arrowY));
        
        // Draw arrow
        float arrowSize = 10.0f;
        float arrowAngle = angle + 3.14159f / 2.0f;
        
        float x1 = arrowX + std::cos(arrowAngle) * arrowSize;
        float y1 = arrowY + std::sin(arrowAngle) * arrowSize;
        float x2 = arrowX + std::cos(arrowAngle + 2.5f) * arrowSize;
        float y2 = arrowY + std::sin(arrowAngle + 2.5f) * arrowSize;
        float x3 = arrowX + std::cos(arrowAngle - 2.5f) * arrowSize;
        float y3 = arrowY + std::sin(arrowAngle - 2.5f) * arrowSize;
        
        primitives->DrawFilledRect(x1, y1, x2 - x1, y2 - y1, color);
        primitives->DrawFilledRect(x1, y1, x3 - x1, y3 - y1, color);
    }

public:
    CESPRenderer() : primitives(nullptr), renderer(nullptr), worldToScreen(nullptr), entityManager(nullptr),
                     screenWidth(1920), screenHeight(1080) {}
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend,
                    WorldToScreen* wts, EntityManager* em) {
        primitives = prim;
        renderer = rend;
        worldToScreen = wts;
        entityManager = em;
    }
    
    void SetScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }
    
    void SetSettings(const ESPSettings& newSettings) {
        settings = newSettings;
    }
    
    ESPSettings GetSettings() const {
        return settings;
    }
    
    void Render() {
        const PlayerData& localPlayer = entityManager->GetLocalPlayer();
        if (!localPlayer.isAlive) return;
        
        auto players = entityManager->GetAllPlayers();
        Vector3 cameraPos = worldToScreen->GetCameraPosition();
        
        // Draw FOV circle
        if (settings.fovCircle) {
            float centerX = screenWidth / 2.0f;
            float centerY = screenHeight / 2.0f;
            float fovRadius = settings.aimbotFOV * 5.0f; // Approximate pixel conversion
            primitives->DrawCircle(centerX, centerY, fovRadius, D3DXCOLOR(1, 1, 1, 0.3f), 32, 1.0f);
        }
        
        for (const auto& player : players) {
            // Skip teammates if not showing them
            if (player.team == localPlayer.team && !settings.showTeammates) continue;
            
            // Skip invalid
            if (!player.isAlive || player.isDormant) continue;
            
            // Determine color
            D3DXCOLOR espColor = (player.team == localPlayer.team) ? 
                                settings.teammateColor : settings.enemyColor;
            
            // Project head position to screen
            Vector3 screenHead;
            bool headOnScreen = worldToScreen->WorldToScreenPoint(player.headPosition, screenHead);
            
            // Project foot position to screen
            Vector3 screenFoot;
            bool footOnScreen = worldToScreen->WorldToScreenPoint(player.position, screenFoot);
            
            // If off-screen and indicators enabled
            if ((!headOnScreen || !footOnScreen) && settings.offScreenIndicators) {
                DrawOffScreenIndicator(player.position, espColor);
                continue;
            }
            
            if (!headOnScreen || !footOnScreen) continue;
            
            // Calculate box dimensions
            float boxHeight = std::abs(screenHead.y - screenFoot.y) * 1.2f;
            float boxWidth = boxHeight * 0.6f;
            
            float boxX = screenHead.x - boxWidth / 2.0f;
            float boxY = screenHead.y;
            
            // Draw corner box
            if (settings.boxes) {
                // Black shadow
                primitives->DrawCornerBox(boxX + 1, boxY + 1, boxWidth, boxHeight, 
                                      D3DXCOLOR(0, 0, 0, 0.5f), 10.0f, 1.0f);
                // Main box
                primitives->DrawCornerBox(boxX, boxY, boxWidth, boxHeight, espColor, 10.0f, 1.0f);
            }
            
            // Draw health bar
            if (settings.healthBar) {
                float barWidth = 4.0f;
                float barHeight = boxHeight;
                float barX = boxX - barWidth - 2.0f;
                float barY = boxY;
                
                // Background
                primitives->DrawFilledRect(barX, barY, barWidth, barHeight, D3DXCOLOR(0, 0, 0, 0.7f));
                primitives->DrawRect(barX, barY, barWidth, barHeight, D3DXCOLOR(0, 0, 0, 1), 1.0f);
                
                // Health fill
                float healthPercent = (float)player.health / 100.0f;
                float fillHeight = barHeight * healthPercent;
                D3DXCOLOR healthColor = GetHealthColor(player.health, 100.0f);
                
                primitives->DrawFilledRect(barX, barY + barHeight - fillHeight, barWidth, fillHeight, healthColor);
                
                // Health text
                char healthStr[8];
                sprintf_s(healthStr, "%d", player.health);
                renderer->DrawText(barX - 25, barY + barHeight / 2 - 6, healthStr, D3DXCOLOR(1, 1, 1, 1), false, renderer->GetSmallFont());
            }
            
            // Draw player name
            if (settings.name) {
                float textWidth, textHeight;
                renderer->GetTextSize(player.name.c_str(), textWidth, textHeight, renderer->GetMainFont());
                float nameX = boxX + boxWidth / 2.0f - textWidth / 2.0f;
                float nameY = boxY - textHeight - 5.0f;
                
                // Shadow
                renderer->DrawTextShadow(nameX + 1, nameY + 1, player.name.c_str(), 
                                       D3DXCOLOR(1, 1, 1, 1), D3DXCOLOR(0, 0, 0, 1), false, renderer->GetMainFont());
                // Main text
                renderer->DrawText(nameX, nameY, player.name.c_str(), D3DXCOLOR(1, 1, 1, 1), false, renderer->GetMainFont());
            }
            
            // Draw weapon name
            if (settings.weapon) {
                std::string weaponName = GetWeaponName(0); // Would need actual weapon ID
                float textWidth, textHeight;
                renderer->GetTextSize(weaponName.c_str(), textWidth, textHeight, renderer->GetSmallFont());
                float weaponX = boxX + boxWidth / 2.0f - textWidth / 2.0f;
                float weaponY = boxY + boxHeight + 5.0f;
                
                renderer->DrawText(weaponX, weaponY, weaponName.c_str(), D3DXCOLOR(0.78f, 0.78f, 0.78f, 1), false, renderer->GetSmallFont());
            }
            
            // Draw distance
            if (settings.distance) {
                char distStr[16];
                sprintf_s(distStr, "%.0fm", player.distance);
                
                float textWidth, textHeight;
                renderer->GetTextSize(distStr, textWidth, textHeight, renderer->GetSmallFont());
                float distX = boxX + boxWidth / 2.0f - textWidth / 2.0f;
                float distY = boxY + boxHeight + 20.0f;
                
                renderer->DrawText(distX, distY, distStr, D3DXCOLOR(0.78f, 0.78f, 0.78f, 1), false, renderer->GetSmallFont());
            }
            
            // Draw snaplines
            if (settings.snaplines) {
                float centerX = screenWidth / 2.0f;
                float centerY = screenHeight / 2.0f;
                
                primitives->DrawLine(centerX, centerY, screenFoot.x, screenFoot.y, espColor, 1.0f);
            }
            
            // Draw head dot
            if (settings.headDot) {
                D3DXCOLOR dotColor = (player.team == localPlayer.team) ? 
                                   settings.teammateColor : D3DXCOLOR(1, 0, 0, 1);
                primitives->DrawFilledCircle(screenHead.x, screenHead.y, 3.0f, dotColor, 16);
            }
        }
    }
};
