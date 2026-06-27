#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "Offsets.h"
#include "EntityManager.h"
#include <cmath>
#include <algorithm>

// Bone selection enum
enum class AimBone {
    HEAD = 6,
    NECK = 5,
    CHEST = 4,
    PELVIS = 3,
    FEET = 0
};

struct AimbotSettings {
    bool enabled = false;
    float smoothing = 2.0f;  // 1.0 = no smoothing, higher = smoother
    float fov = 15.0f;       // FOV threshold in degrees
    AimBone targetBone = AimBone::HEAD;
    float rcsAmount = 2.0f;  // RCS multiplier (0.0 = off, 2.0 = full)
    bool visibilityCheck = false;
    bool targetLock = false; // Lock onto target until death or key release
    int targetPriority = 0;  // 0 = FOV, 1 = Distance
};

class AimbotAdvanced {
private:
    ProcessMemory& process;
    WorldToScreen& worldToScreen;
    EntityManager& entityManager;
    
    AimbotSettings settings;
    
    // Current target
    PlayerData currentTarget;
    bool hasTarget = false;
    
    const float PI = 3.14159265358979323846f;
    
    // Linear interpolation
    float Lerp(float start, float end, float t) {
        return start + (end - start) / t;
    }
    
    // Read current view angles from CInputPtrGlobal + 0x80
    ViewAngles ReadViewAngles() const {
        uintptr_t viewAnglesAddress = process.GetClientDllBase() + Offsets::client_dll::dwCInputPtrGlobal + 0x80;
        
        try {
            float pitch = process.ReadMemory<float>(viewAnglesAddress);
            float yaw = process.ReadMemory<float>(viewAnglesAddress + 0x4);
            return ViewAngles(pitch, yaw);
        } catch (...) {
            return ViewAngles(0, 0);
        }
    }
    
    // Write view angles to CInputPtrGlobal + 0x80
    void WriteViewAngles(const ViewAngles& angles) const {
        uintptr_t viewAnglesAddress = process.GetClientDllBase() + Offsets::client_dll::dwCInputPtrGlobal + 0x80;
        
        try {
            process.WriteMemory<float>(viewAnglesAddress, angles.pitch);
            process.WriteMemory<float>(viewAnglesAddress + 0x4, angles.yaw);
        } catch (...) {
            // Failed to write angles
        }
    }
    
    // Read local player camera position
    Vector3 GetCameraPosition() const {
        return worldToScreen.GetCameraPosition();
    }
    
    // Alternative: read from pawn + 0x1604 (m_vecLastClipCameraPos)
    Vector3 GetCameraPositionFromPawn() const {
        Entity localPawn = entityManager.GetLocalPawn();
        if (!localPawn.IsValid()) return Vector3();
        
        try {
            uintptr_t pawnAddr = localPawn.GetAddress();
            float x = process.ReadMemory<float>(pawnAddr + 0x1604);
            float y = process.ReadMemory<float>(pawnAddr + 0x1604 + 0x4);
            float z = process.ReadMemory<float>(pawnAddr + 0x1604 + 0x8);
            return Vector3(x, y, z);
        } catch (...) {
            return Vector3();
        }
    }
    
    // Calculate aim angles to target position
    ViewAngles CalculateAimAngles(const Vector3& cameraPos, const Vector3& targetPos) {
        // Calculate delta vector
        float deltaX = targetPos.x - cameraPos.x;
        float deltaY = targetPos.y - cameraPos.y;
        float deltaZ = targetPos.z - cameraPos.z;
        
        // Calculate distance
        float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
        
        if (distance < 0.001f) {
            return ViewAngles(0, 0);
        }
        
        // Calculate pitch using asin (alternative method)
        // Pitch = -asin(delta.z / distance) * (180/PI)
        float pitch = -std::asin(deltaZ / distance) * (180.0f / PI);
        
        // Calculate yaw
        // Yaw = atan2(delta.y, delta.x) * (180/PI)
        float yaw = std::atan2(deltaY, deltaX) * (180.0f / PI);
        
        ViewAngles angles(pitch, yaw);
        angles.Normalize();
        
        return angles;
    }
    
    // Calculate FOV to target
    float CalculateFOV(const ViewAngles& currentAngles, const ViewAngles& targetAngles) {
        float deltaPitch = targetAngles.pitch - currentAngles.pitch;
        float deltaYaw = targetAngles.yaw - currentAngles.yaw;
        
        // Normalize yaw delta
        while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
        while (deltaYaw < -180.0f) deltaYaw += 360.0f;
        
        // Calculate FOV as sqrt of squared deltas
        float fov = std::sqrt(deltaPitch * deltaPitch + deltaYaw * deltaYaw);
        
        return fov;
    }
    
    // Apply smoothing to angles
    ViewAngles SmoothAngles(const ViewAngles& current, const ViewAngles& target) {
        ViewAngles smoothed;
        
        // Smooth pitch
        smoothed.pitch = Lerp(current.pitch, target.pitch, settings.smoothing);
        
        // Smooth yaw with shortest path
        float yawDelta = target.yaw - current.yaw;
        
        // Normalize yaw delta to [-180, 180]
        while (yawDelta > 180.0f) yawDelta -= 360.0f;
        while (yawDelta < -180.0f) yawDelta += 360.0f;
        
        smoothed.yaw = current.yaw + yawDelta / settings.smoothing;
        
        smoothed.Normalize();
        
        return smoothed;
    }
    
    // Apply RCS (Recoil Control System)
    ViewAngles ApplyRCS(const ViewAngles& angles, const Vector2& aimPunch) {
        ViewAngles corrected = angles;
        
        // Subtract aim punch * RCS amount
        corrected.pitch -= aimPunch.x * settings.rcsAmount;
        corrected.yaw -= aimPunch.y * settings.rcsAmount;
        
        corrected.Normalize();
        
        return corrected;
    }
    
    // Get target position based on bone selection
    Vector3 GetTargetPosition(const PlayerData& player) {
        switch (settings.targetBone) {
            case AimBone::HEAD:
                return player.headPosition;
            case AimBone::NECK:
                // Neck is between head and chest
                return Vector3(
                    (player.headPosition.x + player.position.x) / 2,
                    (player.headPosition.y + player.position.y) / 2,
                    (player.headPosition.z + player.position.z) / 2
                );
            case AimBone::CHEST:
                return Vector3(
                    player.position.x,
                    player.position.y,
                    player.position.z + 20.0f
                );
            case AimBone::PELVIS:
                return Vector3(
                    player.position.x,
                    player.position.y,
                    player.position.z + 10.0f
                );
            case AimBone::FEET:
                return player.position;
            default:
                return player.headPosition;
        }
    }
    
    // Check if target is visible (using m_iIDEntIndex)
    bool IsVisible(const PlayerData& player) {
        if (!settings.visibilityCheck) return true;
        
        try {
            // Read m_iIDEntIndex from local pawn
            Entity localPawn = entityManager.GetLocalPawn();
            if (!localPawn.IsValid()) return false;
            
            int crosshairEntity = localPawn.GetEntityIndex();
            
            // Check if crosshair entity matches target
            return crosshairEntity == player.entityIndex;
        } catch (...) {
            return false;
        }
    }
    
    // Get local player aim punch
    Vector2 GetLocalAimPunch() const {
        Entity localPawn = entityManager.GetLocalPawn();
        if (!localPawn.IsValid()) return Vector2(0, 0);
        
        return localPawn.GetAimPunch();
    }
    
    // Check if local player is shooting
    bool IsShooting() const {
        Entity localPawn = entityManager.GetLocalPawn();
        if (!localPawn.IsValid()) return false;
        
        return localPawn.GetShotsFired() > 0;
    }

public:
    AimbotAdvanced(ProcessMemory& pm, WorldToScreen& wts, EntityManager& em)
        : process(pm), worldToScreen(wts), entityManager(em), hasTarget(false) {
        currentTarget = PlayerData{};
    }
    
    // Update aimbot settings
    void SetSettings(const AimbotSettings& newSettings) {
        settings = newSettings;
    }
    
    AimbotSettings GetSettings() const {
        return settings;
    }
    
    // Main aim function
    void Aim() {
        if (!settings.enabled) {
            hasTarget = false;
            return;
        }
        
        // Get local player data
        const PlayerData& localPlayer = entityManager.GetLocalPlayer();
        if (!localPlayer.isAlive) {
            hasTarget = false;
            return;
        }
        
        // Get camera position
        Vector3 cameraPos = GetCameraPosition();
        if (cameraPos.Length() < 0.1f) {
            cameraPos = GetCameraPositionFromPawn();
        }
        
        if (cameraPos.Length() < 0.1f) {
            hasTarget = false;
            return;
        }
        
        // Get current view angles
        ViewAngles currentAngles = ReadViewAngles();
        
        // Get valid targets
        auto targets = entityManager.GetValidTargets();
        if (targets.empty()) {
            hasTarget = false;
            return;
        }
        
        // Find best target
        PlayerData bestTarget;
        float bestScore = settings.fov; // FOV threshold
        bool foundTarget = false;
        
        for (const auto& target : targets) {
            // Get target position based on bone selection
            Vector3 targetPos = GetTargetPosition(target);
            
            // Calculate aim angles
            ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
            
            // Calculate FOV
            float fov = CalculateFOV(currentAngles, targetAngles);
            
            // Check FOV threshold
            if (fov > settings.fov) continue;
            
            // Check visibility
            if (!IsVisible(target)) continue;
            
            // Calculate score based on priority
            float score;
            if (settings.targetPriority == 0) {
                // Priority by FOV (lower is better)
                score = fov;
            } else {
                // Priority by distance (closer is better)
                score = target.distance;
            }
            
            if (score < bestScore) {
                bestScore = score;
                bestTarget = target;
                foundTarget = true;
            }
        }
        
        if (!foundTarget) {
            hasTarget = false;
            return;
        }
        
        // Lock onto target if enabled
        if (settings.targetLock && hasTarget) {
            // Keep targeting current target if still valid
            bool targetStillValid = false;
            for (const auto& target : targets) {
                if (target.pawnAddr == currentTarget.pawnAddr) {
                    bestTarget = target;
                    targetStillValid = true;
                    break;
                }
            }
            
            if (!targetStillValid) {
                hasTarget = false;
                return;
            }
        } else {
            currentTarget = bestTarget;
            hasTarget = true;
        }
        
        // Get target position
        Vector3 targetPos = GetTargetPosition(currentTarget);
        
        // Calculate target angles
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
        
        // Apply RCS if shooting
        if (settings.rcsAmount > 0.0f && IsShooting()) {
            Vector2 aimPunch = GetLocalAimPunch();
            targetAngles = ApplyRCS(targetAngles, aimPunch);
        }
        
        // Apply smoothing
        ViewAngles smoothedAngles = SmoothAngles(currentAngles, targetAngles);
        
        // Write angles
        WriteViewAngles(smoothedAngles);
    }
    
    // Reset target
    void ResetTarget() {
        hasTarget = false;
        currentTarget = PlayerData{};
    }
    
    // Check if currently targeting
    bool HasTarget() const {
        return hasTarget;
    }
    
    // Get current target
    const PlayerData& GetCurrentTarget() const {
        return currentTarget;
    }
    
    // Get FOV to specific player
    float GetFOVToPlayer(const PlayerData& player) {
        Vector3 cameraPos = GetCameraPosition();
        ViewAngles currentAngles = ReadViewAngles();
        Vector3 targetPos = GetTargetPosition(player);
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
        
        return CalculateFOV(currentAngles, targetAngles);
    }
};
