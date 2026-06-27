#pragma once

#include <cstdint>

namespace Offsets {
    // client.dll offsets (Build 14165, June 22, 2026)
    namespace client_dll {
        constexpr uintptr_t dwEntityList = 0x24E76A0;
        constexpr uintptr_t dwLocalPlayerController = 0x2320720;
        constexpr uintptr_t dwLocalPlayerPawn = 0x2341698;
        constexpr uintptr_t dwGlobalVars = 0x20616D0;
        constexpr uintptr_t dwViewMatrix = 0x20616D0;  // same as GlobalVars
        constexpr uintptr_t dwCInputPtrGlobal = 0x2079860;
        constexpr uintptr_t dwCSGOInput = 0x2356240;
        constexpr uintptr_t dwViewAngles = 0x2079860;  // CInputPtrGlobal + 0x80
        constexpr uintptr_t dwGameRules = 0x1A02BB8;
        constexpr uintptr_t dwPlantedC4 = 0x234FF98;
        constexpr uintptr_t dwGlowManager = 0x233DF50;
        constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2090;
    }

    // engine2.dll offsets
    namespace engine2_dll {
        constexpr uintptr_t dwBuildNumber = 0x60CC74;
        constexpr uintptr_t dwNetworkGameClient = 0x90A1A0;
        constexpr uintptr_t dwNetworkGameClient_clientTickCount = 0x378;
        constexpr uintptr_t dwNetworkGameClient_deltaTick = 0x24C;
        constexpr uintptr_t dwNetworkGameClient_isBackgroundMap = 0x2C141F;
        constexpr uintptr_t dwNetworkGameClient_localPlayer = 0xF8;
        constexpr uintptr_t dwNetworkGameClient_maxClients = 0x240;
        constexpr uintptr_t dwNetworkGameClient_serverTickCount = 0x24C;
        constexpr uintptr_t dwNetworkGameClient_signOnState = 0x230;
        constexpr uintptr_t dwWindowHeight = 0x90E5C4;
        constexpr uintptr_t dwWindowWidth = 0x90E5C0;
    }

    // inputsystem.dll offsets
    namespace inputsystem_dll {
        constexpr uintptr_t dwInputSystem = 0x42B50;
    }

    // matchmaking.dll offsets
    namespace matchmaking_dll {
        constexpr uintptr_t dwGameTypes = 0x1B0F80;
    }

    // soundsystem.dll offsets
    namespace soundsystem_dll {
        constexpr uintptr_t dwSoundSystem = 0x512360;
        constexpr uintptr_t dwSoundSystem_engineViewData = 0x7C;
    }

    // Schema field offsets (Build 14165)
    namespace schema {
        constexpr uintptr_t m_iHealth = 0x344;
        constexpr uintptr_t m_iTeamNum = 0x3E3;
        constexpr uintptr_t m_hPlayerPawn = 0x824;
        constexpr uintptr_t m_pGameSceneNode = 0x328;
        constexpr uintptr_t m_modelState = 0x170;       // relative to gameSceneNode
        constexpr uintptr_t m_boneArray = 0x80;          // relative to modelState
        constexpr uintptr_t m_vecOrigin = 0x1324;        // m_vOldOrigin
        constexpr uintptr_t m_sSanitizedPlayerName = 0x778;
        constexpr uintptr_t m_iIDEntIndex = 0x3EDC;
        constexpr uintptr_t m_bIsDefusing = 0x23EA;
        constexpr uintptr_t m_ArmorValue = 0x241C;
        constexpr uintptr_t m_flFlashOverlayAlpha = 0x1400;
        constexpr uintptr_t m_bDormant = 0xED;
        constexpr uintptr_t m_lifeState = 0x350;
        constexpr uintptr_t m_aimPunchAngle = 0x23F0;
        constexpr uintptr_t m_iShotsFired = 0x23D8;
    }
}
