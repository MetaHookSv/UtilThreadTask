set(UTILTHREADTASK_DEPENDENCY_CACHE_DIR "${PROJECT_SOURCE_DIR}/thirdparty/cache" CACHE PATH "Downloaded binary dependency cache")
set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches the pinned SDK")
set(VC_LTL_Root "$ENV{VC_LTL_Root}" CACHE PATH "Existing VC-LTL binary package; empty downloads the verified package")

function(utilthreadtask_require_files name source)
    foreach(required IN LISTS ARGN)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "${name} is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(utilthreadtask_prepare_dependencies)
    set(metahook_files include/HLSDK/common/interface.h include/HLSDK/common/interface.cpp LICENSE)
    set(vcltl_files "VC-LTL helper for cmake.cmake" config/config.cmake
        TargetPlatform/6.0.6000.0/lib/Win32/libucrt.lib Readme.md)

    # Validate all explicit paths before downloads. External source trees are read-only.
    if(METAHOOK_SOURCE_PATH)
        get_filename_component(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        utilthreadtask_require_files(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" ${metahook_files})
    endif()
    if(VC_LTL_Root)
        get_filename_component(VC_LTL_Root "${VC_LTL_Root}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        utilthreadtask_require_files(VC_LTL_Root "${VC_LTL_Root}" ${vcltl_files})
    endif()

    if(NOT METAHOOK_SOURCE_PATH)
        include(FetchContent)
        FetchContent_Declare(utilthreadtask_metahook
            GIT_REPOSITORY https://github.com/MetaHookSv/MetaHook
            GIT_TAG 4d23b6fecd79dc949aabc2e145480cd1328d4a35
            GIT_SUBMODULES "" GIT_SUBMODULES_RECURSE FALSE
            # Populate the SDK without configuring the launcher.
            SOURCE_SUBDIR include)
        FetchContent_MakeAvailable(utilthreadtask_metahook)
        set(METAHOOK_SOURCE_PATH "${utilthreadtask_metahook_SOURCE_DIR}")
    endif()
    utilthreadtask_require_files(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" ${metahook_files})
    set(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" PARENT_SCOPE)
    message(STATUS "METAHOOK_SOURCE_PATH: ${METAHOOK_SOURCE_PATH}")

    if(NOT VC_LTL_Root)
        include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VCLTL.cmake")
        set(VC_LTL_Root "${UTILTHREADTASK_DEPENDENCY_CACHE_DIR}/VC-LTL-5.3.1")
        utilthreadtask_prepare_vcltl()
    endif()
    utilthreadtask_require_files(VC_LTL_Root "${VC_LTL_Root}" ${vcltl_files})
    set(VC_LTL_Root "${VC_LTL_Root}" PARENT_SCOPE)
    message(STATUS "VC_LTL_Root: ${VC_LTL_Root}")
endfunction()
