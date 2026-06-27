#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "Offsets.h"
#include <cmath>

struct ViewAngles {
    float pitch; // X axis (up/down), clamped to [-89, 89]
    float yaw;   // Y axis (left/right), normalized to [-180, 180]
    
    ViewAngles() : pitch(0), yaw(0) {}
    ViewAngles(float p, float y) : pitch(p), yaw(y) {}
    
    void Normalize() {
        // Normalize yaw to [-180, 180]
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        
        // Clamp pitch to [-89, 89]
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    
    ViewAngles operator-(const ViewAngles& other) const {
        return ViewAngles(pitch - other.pitch, yaw - other.yaw);
    }
    
    ViewAngles operator+(const ViewAngles& other) const {
        return ViewAngles(pitch + other.pitch, yaw + other.yaw);
    }
};

class Aimbot {
private:
    ProcessMemory& process;
    WorldToScreen& worldToScreen;
    
    float smoothness; // 0.0 to 1.0 (1.0 = instant, 0.0 = no movement)
    
    // Linear interpolation
    static float Lerp(float start, float end, float t) {
        return start + (end - start) * t;
    }
    
    // Calculate head position based on entity origin
    Vector3 GetHeadPosition(const Entity& entity) {
        Vector3 origin = entity.GetPosition();
        
        // Check if entity is crouching (this is a simplified check)
        // In production, you'd read the actual crouch state from entity
        bool isCrouching = false; // TODO: Read crouch state from entity
        
        // Add height offset for head position
        // Standing: +64 units, Crouching: +46 units
        float headHeight = isCrouching ? 46.0f : 64.0f;
        
        return Vector3(origin.x, origin.y, origin.z + headHeight);
    }
    
    // Read current view angles from memory
    ViewAngles ReadViewAngles() const {
        uintptr_t viewAnglesAddress = process.GetClientDllBase() + Offsets::client_dll::dwViewAngles;
        
        try {
            float pitch = process.ReadMemory<float>(viewAnglesAddress);
            float yaw = process.ReadMemory<float>(viewAnglesAddress + 0x4);
            return ViewAngles(pitch, yaw);
        } catch (...) {
            return ViewAngles(0, 0);
        }
    }
    
    // Write view angles to memory
    void WriteViewAngles(const ViewAngles& angles) const {
        uintptr_t viewAnglesAddress = process.GetClientDllBase() + Offsets::client_dll::dwViewAngles;
        
        try {
            process.WriteMemory<float>(viewAnglesAddress, angles.pitch);
            process.WriteMemory<float>(viewAnglesAddress + 0x4, angles.yaw);
        } catch (...) {
            // Failed to write angles
        }
    }
    
public:
    Aimbot(ProcessMemory& pm, WorldToScreen& wts) 
        : process(pm), worldToScreen(wts), smoothness(0.1f) {}
    
    void SetSmoothness(float smooth) {
        smoothness = smooth;
        if (smoothness < 0.0f) smoothness = 0.0f;
        if (smoothness > 1.0f) smoothness = 1.0f;
    }
    
    float GetSmoothness() const {
        return smoothness;
    }
    
    // Calculate aim angles to target
    ViewAngles CalculateAimAngles(const Vector3& cameraPos, const Vector3& targetPos) {
        // Calculate delta vector
        float deltaX = targetPos.x - cameraPos.x;
        float deltaY = targetPos.y - cameraPos.y;
        float deltaZ = targetPos.z - cameraPos.z;
        
        // Calculate horizontal distance
        float horizontalDistance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        
        // Calculate pitch (up/down angle)
        // pitch = -atan2(delta.z, sqrt(delta.x^2 + delta.y^2)) * (180/π)
        float pitch = -std::atan2(deltaZ, horizontalDistance) * (180.0f / 3.14159265358979323846f);
        
        // Calculate yaw (left/right angle)
        // yaw = atan2(delta.y, delta.x) * (180/π)
        float yaw = std::atan2(deltaY, deltaX) * (180.0f / 3.14159265358979323846f);
        
        ViewAngles angles(pitch, yaw);
        angles.Normalize();
        
        return angles;
    }
    
    // Smoothly move current angles to target angles
    ViewAngles SmoothAngles(const ViewAngles& current, const ViewAngles& target) {
        ViewAngles smoothed;
        
        // Smooth pitch
        smoothed.pitch = Lerp(current.pitch, target.pitch, smoothness);
        
        // Smooth yaw with shortest path consideration
        float yawDelta = target.yaw - current.yaw;
        
        // Normalize yaw delta to [-180, 180] for shortest path
        while (yawDelta > 180.0f) yawDelta -= 360.0f;
        while (yawDelta < -180.0f) yawDelta += 360.0f;
        
        smoothed.yaw = current.yaw + yawDelta * smoothness;
        
        smoothed.Normalize();
        
        return smoothed;
    }
    
    // Main aim function - aims at target entity
    void AimAt(const Entity& target) {
        // Get camera position from view matrix
        Vector3 cameraPos = worldToScreen.GetCameraPosition();
        
        // Get target head position
        Vector3 headPos = GetHeadPosition(target);
        
        // Calculate target angles
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, headPos);
        
        // Read current view angles
        ViewAngles currentAngles = ReadViewAngles();
        
        // Apply smoothing
        ViewAngles smoothedAngles = SmoothAngles(currentAngles, targetAngles);
        
        // Write smoothed angles to memory
        WriteViewAngles(smoothedAngles);
    }
    
    // Aim at specific world position
    void AimAtPosition(const Vector3& targetPos) {
        // Get camera position from view matrix
        Vector3 cameraPos = worldToScreen.GetCameraPosition();
        
        // Calculate target angles
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
        
        // Read current view angles
        ViewAngles currentAngles = ReadViewAngles();
        
        // Apply smoothing
        ViewAngles smoothedAngles = SmoothAngles(currentAngles, targetAngles);
        
        // Write smoothed angles to memory
        WriteViewAngles(smoothedAngles);
    }
    
    // Get the angle difference between current view and target
    ViewAngles GetAngleDifference(const Entity& target) {
        Vector3 cameraPos = worldToScreen.GetCameraPosition();
        Vector3 headPos = GetHeadPosition(target);
        
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, headPos);
        ViewAngles currentAngles = ReadViewAngles();
        
        ViewAngles diff = targetAngles - currentAngles;
        diff.Normalize();
        
        return diff;
    }
    
    // Check if target is within FOV
    bool IsInFOV(const Entity& target, float fovThreshold = 15.0f) {
        ViewAngles diff = GetAngleDifference(target);
        
        // Calculate total angle difference
        float totalDiff = std::sqrt(diff.pitch * diff.pitch + diff.yaw * diff.yaw);
        
        return totalDiff < fovThreshold;
    }
    
    // Get the best target (closest to crosshair within FOV)
    Entity GetBestTarget(const std::vector<Entity>& entities, float fovThreshold = 15.0f) {
        Entity bestTarget(nullptr, 0);
        float bestDiff = fovThreshold;
        
        for (const auto& entity : entities) {
            if (!entity.IsValid() || !entity.IsAlive()) continue;
            
            ViewAngles diff = GetAngleDifference(entity);
            float totalDiff = std::sqrt(diff.pitch * diff.pitch + diff.yaw * diff.yaw);
            
            if (totalDiff < bestDiff) {
                bestDiff = totalDiff;
                bestTarget = entity;
            }
        }
        
        return bestTarget;
    }
    
    // Reset view angles to current (stop aiming)
    void Reset() {
        ViewAngles current = ReadViewAngles();
        WriteViewAngles(current);
    }
};
