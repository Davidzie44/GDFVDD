#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "Offsets.h"
#include "EntityManager.h"
#include "VisibilityCheck.h"
#include <cmath>
#include <algorithm>
#include <windows.h>

extern std::ofstream g_log;
void LogFmt(const char* fmt, ...);

enum class AimBone {
    HEAD = 0,
    NECK = 1,
    CHEST = 2,
    PELVIS = 3,
    FEET = 4
};

struct AimbotSettings {
    bool enabled = false;
    float smoothing = 1.0f;
    float fov = 30.0f;
    AimBone targetBone = AimBone::HEAD;
    float rcsAmount = 2.0f;
    bool visibilityCheck = false;
    bool targetLock = false;
    int targetPriority = 0;
    bool triggerbotEnabled = false;
    bool triggerbotHeadOnly = false;
    bool rapidFireEnabled = false;
    float rapidFireSpeed = 8.0f;
};

class AimbotAdvanced {
private:
    ProcessMemory& process;
    WorldToScreen& worldToScreen;
    EntityManager& entityManager;
    VisibilityCheck& visibilityCheck;
    AimbotSettings settings;
    PlayerData currentTarget;
    bool hasTarget = false;

    const float PI = 3.14159265358979323846f;

    ViewAngles ReadViewAngles() const {
        uintptr_t addr = process.GetClientDllBase() + Offsets::client_dll::dwViewAngles;
        try {
            float pitch = process.ReadMemory<float>(addr);
            float yaw = process.ReadMemory<float>(addr + 0x4);
            return ViewAngles(pitch, yaw);
        } catch (...) { return ViewAngles(0, 0); }
    }

    void WriteViewAngles(const ViewAngles& angles) const {
        uintptr_t addr = process.GetClientDllBase() + Offsets::client_dll::dwCSGOInput + 0x688;
        try {
            process.WriteMemory<float>(addr, angles.pitch);
            process.WriteMemory<float>(addr + 0x4, angles.yaw);
        } catch (...) {}
    }

    Vector3 GetCameraPosition() const {
        const PlayerData& local = entityManager.GetLocalPlayer();
        if (local.pawnAddr != 0 && local.position.Length() > 0.1f) {
            return Vector3(local.position.x, local.position.y, local.position.z + 64.0f);
        }
        return worldToScreen.GetCameraPosition();
    }

    ViewAngles CalculateAimAngles(const Vector3& cameraPos, const Vector3& targetPos) {
        float dx = targetPos.x - cameraPos.x;
        float dy = targetPos.y - cameraPos.y;
        float dz = targetPos.z - cameraPos.z;
        float dist2D = std::sqrt(dx * dx + dy * dy);
        float pitch = -std::atan2(dz, dist2D) * (180.0f / PI);
        float yaw = std::atan2(dy, dx) * (180.0f / PI);
        ViewAngles angles(pitch, yaw);
        angles.Normalize();
        return angles;
    }

    float CalculateFOV(const ViewAngles& current, const ViewAngles& target) {
        float dp = target.pitch - current.pitch;
        float dy = target.yaw - current.yaw;
        while (dy > 180.0f) dy -= 360.0f;
        while (dy < -180.0f) dy += 360.0f;
        return std::sqrt(dp * dp + dy * dy);
    }

    ViewAngles SmoothAngles(const ViewAngles& current, const ViewAngles& target) {
        ViewAngles smoothed;
        float sm = settings.smoothing;
        if (sm < 1.0f) sm = 1.0f;
        smoothed.pitch = current.pitch + (target.pitch - current.pitch) / sm;
        float yawDelta = target.yaw - current.yaw;
        while (yawDelta > 180.0f) yawDelta -= 360.0f;
        while (yawDelta < -180.0f) yawDelta += 360.0f;
        smoothed.yaw = current.yaw + yawDelta / sm;
        smoothed.Normalize();
        return smoothed;
    }

    ViewAngles ApplyRCS(const ViewAngles& angles, const Vector2& aimPunch) {
        ViewAngles corrected = angles;
        corrected.pitch -= aimPunch.x * settings.rcsAmount;
        corrected.yaw -= aimPunch.y * settings.rcsAmount;
        corrected.Normalize();
        return corrected;
    }

    Vector3 GetTargetPosition(const PlayerData& player) {
        switch (settings.targetBone) {
            case AimBone::HEAD:  return Vector3(player.position.x, player.position.y, player.position.z + 64.0f);
            case AimBone::NECK:  return Vector3(player.position.x, player.position.y, player.position.z + 56.0f);
            case AimBone::CHEST: return Vector3(player.position.x, player.position.y, player.position.z + 40.0f);
            case AimBone::PELVIS:return Vector3(player.position.x, player.position.y, player.position.z + 20.0f);
            case AimBone::FEET:  return player.position;
            default:             return Vector3(player.position.x, player.position.y, player.position.z + 64.0f);
        }
    }

    bool IsVisible(const PlayerData& player) {
        if (!settings.visibilityCheck) return true;

        // Use EngineTrace if available (proper wall check)
        if (visibilityCheck.IsAvailable()) {
            Vector3 eyePos = GetCameraPosition();
            Vector3 targetPos = GetTargetPosition(player);
            return visibilityCheck.IsVisible(eyePos, targetPos);
        }

        // Fallback: crosshair-based check
        try {
            Entity localPawn = entityManager.GetLocalPawn();
            if (!localPawn.IsValid()) return false;
            int crosshairEntity = process.ReadMemory<int>(localPawn.GetAddress() + Offsets::schema::m_iIDEntIndex);
            return crosshairEntity == player.entityIndex;
        } catch (...) { return false; }
    }

    Vector2 GetLocalAimPunch() const {
        Entity localPawn = entityManager.GetLocalPawn();
        if (!localPawn.IsValid()) return Vector2(0, 0);
        try {
            uintptr_t aimPunchSvc = process.ReadMemory<uintptr_t>(
                localPawn.GetAddress() + Offsets::schema::m_pAimPunchServices);
            if (aimPunchSvc == 0) return Vector2(0, 0);
            float x = process.ReadMemory<float>(aimPunchSvc + Offsets::schema::m_predictableBaseAngle);
            float y = process.ReadMemory<float>(aimPunchSvc + Offsets::schema::m_predictableBaseAngle + 0x4);
            return Vector2(x, y);
        } catch (...) { return Vector2(0, 0); }
    }

    bool IsShooting() const {
        Entity localPawn = entityManager.GetLocalPawn();
        if (!localPawn.IsValid()) return false;
        try {
            return process.ReadMemory<int>(localPawn.GetAddress() + Offsets::schema::m_iShotsFired) > 0;
        } catch (...) { return false; }
    }

    uintptr_t ResolveEntityByIndex(int entityIndex) {
        uintptr_t elBase = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwEntityList
        );
        if (elBase == 0 || entityIndex < 0 || entityIndex > 2047) return 0;
        try {
            uint32_t pageIndex = entityIndex >> 9;
            uint32_t pageOffset = entityIndex & 0x1FF;
            uintptr_t pagePtr = process.ReadMemory<uintptr_t>(
                elBase + 0x10 + (uintptr_t)pageIndex * 8
            );
            if (pagePtr == 0) return 0;
            return process.ReadMemory<uintptr_t>(pagePtr + (uintptr_t)pageOffset * 0x70);
        } catch (...) { return 0; }
    }

    void SendMouseDown() {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
    }

    void SendMouseUp() {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

public:
    AimbotAdvanced(ProcessMemory& pm, WorldToScreen& wts, EntityManager& em, VisibilityCheck& vc)
        : process(pm), worldToScreen(wts), entityManager(em), visibilityCheck(vc), hasTarget(false) {
        currentTarget = PlayerData{};
    }

    void SetSettings(const AimbotSettings& newSettings) { settings = newSettings; }
    AimbotSettings GetSettings() const { return settings; }

    void Aim() {
        if (!settings.enabled) { hasTarget = false; return; }

        const PlayerData& localPlayer = entityManager.GetLocalPlayer();
        if (!localPlayer.isAlive) { hasTarget = false; return; }

        Vector3 cameraPos = GetCameraPosition();
        if (cameraPos.Length() < 0.1f) { hasTarget = false; return; }

        ViewAngles currentAngles = ReadViewAngles();
        auto targets = entityManager.GetValidTargets();
        if (targets.empty()) { hasTarget = false; return; }

        // If we have a locked target, check if it's still valid (alive + in FOV + visible)
        if (settings.targetLock && hasTarget) {
            bool stillValid = false;
            for (const auto& t : targets) {
                if (t.pawnAddr == currentTarget.pawnAddr) {
                    if (t.isAlive && t.health > 0) {
                        Vector3 targetPos = GetTargetPosition(t);
                        ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
                        float fov = CalculateFOV(currentAngles, targetAngles);
                        bool inFov = fov <= settings.fov;
                        bool visible = !settings.visibilityCheck || IsVisible(t);
                        if (inFov && visible) {
                            currentTarget = t;
                            stillValid = true;
                        }
                    }
                    break;
                }
            }
            if (!stillValid) {
                hasTarget = false;
            }
        }

        // Find new target if we don't have one
        if (!hasTarget) {
            PlayerData bestTarget;
            float bestScore = settings.fov;
            bool foundTarget = false;

            for (const auto& target : targets) {
                if (target.isDormant) continue;

                Vector3 targetPos = GetTargetPosition(target);
                ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);
                float fov = CalculateFOV(currentAngles, targetAngles);
                if (fov > settings.fov) continue;

                if (settings.visibilityCheck && !IsVisible(target)) continue;

                float score = (settings.targetPriority == 0) ? fov : target.distance;
                if (score < bestScore) {
                    bestScore = score;
                    bestTarget = target;
                    foundTarget = true;
                }
            }

            if (foundTarget) {
                currentTarget = bestTarget;
                hasTarget = true;
            } else {
                return;
            }
        }

        if (!hasTarget) return;

        Vector3 targetPos = GetTargetPosition(currentTarget);
        ViewAngles targetAngles = CalculateAimAngles(cameraPos, targetPos);

        if (settings.rcsAmount > 0.0f && IsShooting()) {
            Vector2 aimPunch = GetLocalAimPunch();
            targetAngles = ApplyRCS(targetAngles, aimPunch);
        }

        ViewAngles smoothedAngles = SmoothAngles(currentAngles, targetAngles);
        WriteViewAngles(smoothedAngles);
    }

    void Triggerbot() {
        if (!settings.triggerbotEnabled) return;

        const PlayerData& localPlayer = entityManager.GetLocalPlayer();
        if (!localPlayer.isAlive) return;

        try {
            Entity localPawn = entityManager.GetLocalPawn();
            if (!localPawn.IsValid()) return;

            int crosshairEntIdx = process.ReadMemory<int>(localPawn.GetAddress() + Offsets::schema::m_iIDEntIndex);
            if (crosshairEntIdx <= 0 || crosshairEntIdx > 2047) return;

            uintptr_t entityAddr = ResolveEntityByIndex(crosshairEntIdx);
            if (entityAddr == 0) return;

            int hp = process.ReadMemory<int>(entityAddr + Offsets::schema::m_iHealth);
            int tm = process.ReadMemory<int>(entityAddr + Offsets::schema::m_iTeamNum);
            if (hp <= 0 || hp > 300) return;
            if (tm == localPlayer.team) return;

            SendMouseDown();
            Sleep(16);
            SendMouseUp();
        } catch (...) {}
    }

    void RapidFire() {
        if (!settings.rapidFireEnabled) return;

        const PlayerData& localPlayer = entityManager.GetLocalPlayer();
        if (!localPlayer.isAlive) return;

        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            SendMouseUp();
            Sleep(1);
            SendMouseDown();
        }
    }

    void ResetTarget() { hasTarget = false; currentTarget = PlayerData{}; }
    bool HasTarget() const { return hasTarget; }
    const PlayerData& GetCurrentTarget() const { return currentTarget; }
};
