set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches the pinned SDK")

function(utilthreadtask_require_files name source)
    foreach(required IN LISTS ARGN)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "${name} is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(utilthreadtask_prepare_dependencies)
    set(metahook_files include/HLSDK/common/interface.h include/HLSDK/common/interface.cpp LICENSE)

    # Validate all explicit paths before downloads. External source trees are read-only.
    if(METAHOOK_SOURCE_PATH)
        get_filename_component(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        utilthreadtask_require_files(METAHOOK_SOURCE_PATH "${METAHOOK_SOURCE_PATH}" ${metahook_files})
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
endfunction()
