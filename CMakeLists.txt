cmake_minimum_required(VERSION 3.20)
project(LowLevelCPP_XMC LANGUAGES CXX)

add_library(LowLevelCPP_XMC INTERFACE)
add_library(LowLevelCPP::XMC ALIAS LowLevelCPP_XMC)

target_compile_features(LowLevelCPP_XMC INTERFACE cxx_std_20)

target_sources(
    LowLevelCPP_XMC
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Devices/ADCs/XMCVADC.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Devices/DACs/XMCDAC.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Devices/I2CControllers/XMCI2CAccess.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Devices/PIOs/XMCIOPin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/Devices/SPIControllers/XMCSPIAccess.h
)

target_include_directories(
    LowLevelCPP_XMC
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Devices/ADCs>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Devices/DACs>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Devices/I2CControllers>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Devices/PIOs>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Devices/SPIControllers>
)

if(NOT TARGET LowLevelEmbedded)
    if(NOT COMMAND CPMAddPackage)
        set(CPM_VERSION 0.43.1)
        message("Downloading CPM Package Manager version ${CPM_VERSION}")
        file(
            DOWNLOAD
            https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_VERSION}/CPM.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/CPM_${CPM_VERSION}.cmake
        )
        include("${CMAKE_CURRENT_BINARY_DIR}/CPM_${CPM_VERSION}.cmake")
    endif()

    CPMAddPackage(
        NAME LowLevelCPPClasses
        GITHUB_REPOSITORY JeroenVandezande/LowLevelCPPClasses
        GIT_TAG v3.9.0
    )
endif()

target_link_libraries(LowLevelCPP_XMC INTERFACE LowLevelEmbedded)
