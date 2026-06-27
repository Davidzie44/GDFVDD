#pragma once

#include "PrimitivesRenderer.h"
#include "FontRenderer.h"
#include "EntityManager.h"
#include "AimbotAdvanced.h"
#include "Common.h"
#include <string>

class CWatermark {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    
    D3DXCOLOR backgroundColor;
    D3DXCOLOR textColor;
    D3DXCOLOR accentColor;
    
    std::string version;
    std::string build;

public:
    CWatermark() : primitives(nullptr), renderer(nullptr) {
        backgroundColor = D3DXCOLOR(0, 0, 0, 120.0f/255.0f);
        textColor = D3DXCOLOR(1, 1, 1, 180.0f/255.0f);
        accentColor = D3DXCOLOR(0, 170.0f/255.0f, 1, 1);
        version = "CS2 SDK v1.0";
        build = "Build 14165";
    }
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend) {
        primitives = prim;
        renderer = rend;
    }
    
    void Render(float fps) {
        float x = 10.0f;
        float y = 10.0f;
        
        // Calculate text dimensions
        float textWidth, textHeight;
        renderer->GetTextSize(version.c_str(), textWidth, textHeight, renderer->GetMainFont());
        
        float buildWidth, buildHeight;
        renderer->GetTextSize(build.c_str(), buildWidth, buildHeight, renderer->GetSmallFont());
        
        float maxWidth = (textWidth > buildWidth) ? textWidth : buildWidth;
        float totalHeight = textHeight + buildHeight + 8;
        
        // Draw background pill
        float padding = 8.0f;
        float bgWidth = maxWidth + padding * 2;
        float bgHeight = totalHeight + padding * 2;
        
        primitives->DrawFilledRect(x, y, bgWidth, bgHeight, backgroundColor);
        primitives->DrawRect(x, y, bgWidth, bgHeight, D3DXCOLOR(1, 1, 1, 0.3f), 1.0f);
        
        // Draw version
        renderer->DrawText(x + padding, y + padding, version.c_str(), textColor, false, renderer->GetMainFont());
        
        // Draw build and FPS
        char fpsStr[32];
        sprintf_s(fpsStr, "%s | FPS: %.0f", build.c_str(), fps);
        renderer->DrawText(x + padding, y + padding + textHeight + 4, fpsStr, textColor, false, renderer->GetSmallFont());
    }
};

class CInfoPanel {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    EntityManager* entityManager;
    AimbotAdvanced* aimbot;
    
    D3DXCOLOR backgroundColor;
    D3DXCOLOR textColor;
    D3DXCOLOR accentColor;
    
    bool isVisible;

public:
    CInfoPanel() : primitives(nullptr), renderer(nullptr), entityManager(nullptr), aimbot(nullptr),
                   isVisible(false) {
        backgroundColor = D3DXCOLOR(20.0f/255.0f, 20.0f/255.0f, 25.0f/255.0f, 200.0f/255.0f);
        textColor = D3DXCOLOR(1, 1, 1, 1);
        accentColor = D3DXCOLOR(0, 170.0f/255.0f, 1, 1);
    }
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend, EntityManager* em, AimbotAdvanced* ab) {
        primitives = prim;
        renderer = rend;
        entityManager = em;
        aimbot = ab;
    }
    
    void SetVisible(bool visible) {
        isVisible = visible;
    }
    
    void Render(int screenWidth) {
        if (!isVisible) return;
        
        // Check if aimbot has target
        bool hasTarget = aimbot && aimbot->HasTarget();
        
        std::string infoText;
        if (hasTarget) {
            const PlayerData& target = aimbot->GetCurrentTarget();
            char targetStr[128];
            sprintf_s(targetStr, "Target: %s | Distance: %.0fm | Health: %dHP", 
                     target.name.c_str(), target.distance, target.health);
            infoText = targetStr;
        } else {
            infoText = "No target";
        }
        
        // Calculate dimensions
        float textWidth, textHeight;
        renderer->GetTextSize(infoText.c_str(), textWidth, textHeight, renderer->GetMainFont());
        
        float padding = 12.0f;
        float panelWidth = textWidth + padding * 2;
        float panelHeight = textHeight + padding * 2;
        
        float x = (screenWidth - panelWidth) / 2.0f;
        float y = 50.0f;
        
        // Draw background
        primitives->DrawFilledRect(x, y, panelWidth, panelHeight, backgroundColor);
        
        // Draw accent border (left side)
        primitives->DrawFilledRect(x, y, 2.0f, panelHeight, accentColor);
        
        // Draw text
        renderer->DrawText(x + padding + 4, y + padding, infoText.c_str(), textColor, false, renderer->GetMainFont());
    }
};

class CCrosshair {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    AimbotAdvanced* aimbot;
    
    D3DXCOLOR color;
    D3DXCOLOR shadowColor;
    D3DXCOLOR lockedColor;
    
    float size;
    float thickness;
    float gap;
    float dotRadius;

public:
    CCrosshair() : primitives(nullptr), renderer(nullptr), aimbot(nullptr) {
        color = D3DXCOLOR(1, 1, 1, 200.0f/255.0f);
        shadowColor = D3DXCOLOR(0, 0, 0, 1);
        lockedColor = D3DXCOLOR(0, 170.0f/255.0f, 1, 1);
        size = 8.0f;
        thickness = 2.0f;
        gap = 4.0f;
        dotRadius = 3.0f;
    }
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend, AimbotAdvanced* ab) {
        primitives = prim;
        renderer = rend;
        aimbot = ab;
    }
    
    void Render(int screenWidth, int screenHeight) {
        float centerX = screenWidth / 2.0f;
        float centerY = screenHeight / 2.0f;
        
        // Check if aimbot is locked
        bool isLocked = aimbot && aimbot->HasTarget();
        D3DXCOLOR currentColor = isLocked ? lockedColor : color;
        
        // Draw crosshair lines with shadow
        // Top
        primitives->DrawLine(centerX, centerY - gap - size, centerX, centerY - gap, shadowColor, thickness + 2);
        primitives->DrawLine(centerX, centerY - gap - size, centerX, centerY - gap, currentColor, thickness);
        
        // Bottom
        primitives->DrawLine(centerX, centerY + gap, centerX, centerY + gap + size, shadowColor, thickness + 2);
        primitives->DrawLine(centerX, centerY + gap, centerX, centerY + gap + size, currentColor, thickness);
        
        // Left
        primitives->DrawLine(centerX - gap - size, centerY, centerX - gap, centerY, shadowColor, thickness + 2);
        primitives->DrawLine(centerX - gap - size, centerY, centerX - gap, centerY, currentColor, thickness);
        
        // Right
        primitives->DrawLine(centerX + gap, centerY, centerX + gap + size, centerY, shadowColor, thickness + 2);
        primitives->DrawLine(centerX + gap, centerY, centerX + gap + size, centerY, currentColor, thickness);
        
        // Draw center dot
        primitives->DrawFilledCircle(centerX, centerY, dotRadius, shadowColor, 16);
        primitives->DrawFilledCircle(centerX, centerY, dotRadius - 1, currentColor, 16);
    }
};

class CKillfeed {
private:
    CD3D11Primitives* primitives;
    CD3D11Renderer* renderer;
    EntityManager* entityManager;
    
    D3DXCOLOR backgroundColor;
    D3DXCOLOR textColor;
    
    bool isVisible;

public:
    CKillfeed() : primitives(nullptr), renderer(nullptr), entityManager(nullptr), isVisible(false) {
        backgroundColor = D3DXCOLOR(0, 0, 0, 150.0f/255.0f);
        textColor = D3DXCOLOR(1, 1, 1, 1);
    }
    
    void Initialize(CD3D11Primitives* prim, CD3D11Renderer* rend, EntityManager* em) {
        primitives = prim;
        renderer = rend;
        entityManager = em;
    }
    
    void SetVisible(bool visible) {
        isVisible = visible;
    }
    
    void Render(int screenWidth, int screenHeight) {
        if (!isVisible) return;
        
        // Get player count
        auto players = entityManager->GetAllPlayers();
        int aliveCount = 0;
        for (const auto& player : players) {
            if (player.isAlive) aliveCount++;
        }
        
        char infoStr[32];
        sprintf_s(infoStr, "Players alive: %d/10", aliveCount);
        
        // Calculate dimensions
        float textWidth, textHeight;
        renderer->GetTextSize(infoStr, textWidth, textHeight, renderer->GetSmallFont());
        
        float padding = 8.0f;
        float panelWidth = textWidth + padding * 2;
        float panelHeight = textHeight + padding * 2;
        
        float x = screenWidth - panelWidth - 10.0f;
        float y = screenHeight - panelHeight - 10.0f;
        
        // Draw background
        primitives->DrawFilledRect(x, y, panelWidth, panelHeight, backgroundColor);
        primitives->DrawRect(x, y, panelWidth, panelHeight, D3DXCOLOR(1, 1, 1, 0.3f), 1.0f);
        
        // Draw text
        renderer->DrawText(x + padding, y + padding, infoStr, textColor, false, renderer->GetSmallFont());
    }
};

class CHUDRenderer {
private:
    CWatermark watermark;
    CInfoPanel infoPanel;
    CCrosshair crosshair;
    CKillfeed killfeed;
    
    float fps;
    int frameCount;
    DWORD lastFpsUpdate;
    DWORD frameStartTime;

public:
    CHUDRenderer() : fps(0), frameCount(0), lastFpsUpdate(0), frameStartTime(0) {}
    
    void Initialize(CD3D11Primitives* primitives, CD3D11Renderer* renderer,
                   EntityManager* entityManager, AimbotAdvanced* aimbot) {
        watermark.Initialize(primitives, renderer);
        infoPanel.Initialize(primitives, renderer, entityManager, aimbot);
        crosshair.Initialize(primitives, renderer, aimbot);
        killfeed.Initialize(primitives, renderer, entityManager);
        
        frameStartTime = GetTickCount();
    }
    
    void UpdateFPS() {
        frameCount++;
        DWORD currentTime = GetTickCount();
        
        if (currentTime - lastFpsUpdate >= 1000) {
            fps = frameCount * 1000.0f / (currentTime - lastFpsUpdate);
            frameCount = 0;
            lastFpsUpdate = currentTime;
        }
    }
    
    void Render(int screenWidth, int screenHeight, bool menuOpen) {
        UpdateFPS();
        
        // Always render watermark
        watermark.Render(fps);
        
        // Render info panel when menu is open
        infoPanel.SetVisible(menuOpen);
        infoPanel.Render(screenWidth);
        
        // Always render crosshair
        crosshair.Render(screenWidth, screenHeight);
        
        // Render killfeed (optional)
        killfeed.Render(screenWidth, screenHeight);
    }
    
    CWatermark* GetWatermark() { return &watermark; }
    CInfoPanel* GetInfoPanel() { return &infoPanel; }
    CCrosshair* GetCrosshair() { return &crosshair; }
    CKillfeed* GetKillfeed() { return &killfeed; }
};
