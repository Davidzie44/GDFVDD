#pragma once

#include <cstdint>

namespace Offsets {
    // client.dll offsets (Build 14165, June 22, 2026)
    namespace client_dll {
        constexpr uintptr_t dwEntityList = 0x24E76A0;
        constexpr uintptr_t dwLocalPlayerController = 0x2320720;
        constexpr uintptr_t dwLocalPlayerPawn = 0x2341698;
        constexpr uintptr_t dwGlobalVars = 0x20616D0;
        constexpr uintptr_t dwViewMatrix = 0x20616D0;
        constexpr uintptr_t dwCInputPtrGlobal = 0x2079860;
        constexpr uintptr_t dwViewAngles = 0x2079860;
        constexpr uintptr_t dwCSGOInput = 0x2356240;
        constexpr uintptr_t dwGameRules = 0x1A02BB8;
        constexpr uintptr_t dwPlantedC4 = 0x234FF98;
        constexpr uintptr_t dwGlowManager = 0x233DF50;
        constexpr uintptr_t dwGameEntitySystem = 0x24E76A0;
        constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2090;
    }

    // engine2.dll offsets
    namespace engine2_dll {
        constexpr uintptr_t dwNetworkGameClient = 0x5411C0;
        constexpr uintptr_t dwNetworkGameClient_localPlayer = 0xF0;
        constexpr uintptr_t dwNetworkGameClient_clientTickCount = 0x368;
    }

    // Schema field offsets (Build 14165)
    namespace schema {
        constexpr uintptr_t m_iHealth = 0x344;
        constexpr uintptr_t m_iTeamNum = 0x3E3;
        constexpr uintptr_t m_lifeState = 0x350;
        constexpr uintptr_t m_bDormant = 0xED;
        constexpr uintptr_t m_pGameSceneNode = 0x328;
        constexpr uintptr_t m_modelState = 0x170;
        constexpr uintptr_t m_boneArray = 0x80;
        constexpr uintptr_t m_vecOrigin = 0x1324;
        constexpr uintptr_t m_iIDEntIndex = 0x3EDC;
        constexpr uintptr_t m_flFlashOverlayAlpha = 0x1400;
        constexpr uintptr_t m_aimPunchAngle = 0x23F0;
        constexpr uintptr_t m_iShotsFired = 0x23D8;
        constexpr uintptr_t m_hPlayerPawn = 0x824;
        constexpr uintptr_t m_sSanitizedPlayerName = 0x778;
        constexpr uintptr_t m_ArmorValue = 0x241C;
        constexpr uintptr_t m_bIsDefusing = 0x23EA;
        constexpr uintptr_t m_bHasDefuser = 0x23E9;
        constexpr uintptr_t m_vecLastClipCameraPos = 0x1604;
        constexpr uintptr_t m_angEyeAngles = 0x16A0;
    }
}
