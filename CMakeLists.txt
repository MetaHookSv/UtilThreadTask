cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(UtilThreadTask LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "UtilThreadTask requires Windows MSVC x86. Configure with -A Win32.")
endif()
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select Debug or Release.")
endif()

include(cmake/Dependencies.cmake)
utilthreadtask_prepare_dependencies()
include("${VC_LTL_Root}/VC-LTL helper for cmake.cmake")

add_library(utilthreadtask_settings INTERFACE)
target_compile_features(utilthreadtask_settings INTERFACE cxx_std_20)
target_include_directories(utilthreadtask_settings INTERFACE
    "${PROJECT_SOURCE_DIR}/include/Interface"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common")
target_compile_definitions(utilthreadtask_settings INTERFACE
    WIN32 _WINDOWS UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG> $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(utilthreadtask_settings INTERFACE /W3 /permissive- /wd4819 /Zi)

add_library(UtilThreadTask SHARED
    src/ThreadedTask.cpp src/UtilThreadTask.cpp src/dllmain.cpp
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
target_link_libraries(UtilThreadTask PRIVATE utilthreadtask_settings)
target_compile_definitions(UtilThreadTask PRIVATE UTILTHREADTASK_EXPORTS _USRDLL)
target_compile_options(UtilThreadTask PRIVATE /MP /sdl $<$<CONFIG:Release>:/Gy;/Oi>)
target_link_options(UtilThreadTask PRIVATE /DEBUG /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>)
set_target_properties(UtilThreadTask PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

include(CTest)
if(BUILD_TESTING)
    add_executable(UtilThreadTaskTests tests/RegressionTests.cpp
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
    target_link_libraries(UtilThreadTaskTests PRIVATE utilthreadtask_settings)
    target_compile_definitions(UtilThreadTaskTests PRIVATE NOMINMAX)
    set_target_properties(UtilThreadTaskTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_dependencies(UtilThreadTaskTests UtilThreadTask)
    foreach(scenario Factory Empty Ordering Readiness Limits Drain Destroy CreatorThread ConcurrentQueue ReentrantQueue)
        add_test(NAME ${scenario} COMMAND UtilThreadTaskTests "$<TARGET_FILE:UtilThreadTask>" ${scenario})
        set_tests_properties(${scenario} PROPERTIES TIMEOUT 30)
    endforeach()
endif()

install(TARGETS UtilThreadTask RUNTIME DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_PDB_FILE:UtilThreadTask>" DESTINATION svencoop/metahook/dlls)
install(FILES include/Interface/IUtilThreadTask.h DESTINATION include/Interface)
install(FILES "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.h" DESTINATION include/HLSDK/common)
install(FILES README.md README.zh-CN.md LICENSE THIRD-PARTY-NOTICES.md DESTINATION .)
install(FILES "${METAHOOK_SOURCE_PATH}/LICENSE" DESTINATION licenses/MetaHook)
install(FILES "${VC_LTL_Root}/Readme.md" DESTINATION licenses/VC-LTL)
