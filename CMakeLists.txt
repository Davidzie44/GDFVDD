cmake_minimum_required(VERSION 3.15)
project(CS2ExternalTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    main.cpp
)

# Header files (for IDE purposes)
set(HEADERS
    ProcessMemory.h
    Offsets.h
    CS2Entities.h
    WorldToScreen.h
    PatternScanner.h
    EntityManager.h
    AimbotAdvanced.h
    MenuSystem.h
    ESPRenderer.h
    HUDRenderer.h
    PrimitivesRenderer.h
    FontRenderer.h
    Config.h
    Overlay.h
    ESP.h
    Aimbot.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        d3d11.lib
        dxgi.lib
        gdi32.lib
        dwmapi.lib
        d3dcompiler.lib
    )
    
    # Set subsystem to Windows (no console)
    # Comment out the next line if you want to see console output
    # set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    
    # Set output directory
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Compiler flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /permissive-
    )
endif()
