# CoreMach engine bridge.
#
# CoreMach (https://github.com/Palimbit/CoreMach) is consumed as the pinned
# submodule in external/coremach. Only the engine, runtime, components and
# systems that BluMach runs are added; CoreMach's own project file, tests,
# frontends, catalogue and install rules are deliberately not used.

include_guard(GLOBAL)

set(BLUMACH_COREMACH "AUTO" CACHE STRING "Build the CoreMach engine bridge (AUTO, ON or OFF)")
set_property(CACHE BLUMACH_COREMACH PROPERTY STRINGS AUTO ON OFF)

set(COREMACH_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/coremach")
set(BLUMACH_COREMACH_ENABLED OFF)

if(NOT BLUMACH_COREMACH MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "BLUMACH_COREMACH must be AUTO, ON or OFF")
elseif(BLUMACH_COREMACH STREQUAL "OFF")
    message(STATUS "CoreMach bridge: disabled (BLUMACH_COREMACH=OFF)")
    return()
elseif(NOT EXISTS "${COREMACH_SOURCE_DIR}/CMakeLists.txt")
    if(BLUMACH_COREMACH STREQUAL "ON")
        message(FATAL_ERROR "BLUMACH_COREMACH=ON but external/coremach is missing; run "
                            "`git submodule update --init external/coremach`")
    endif()
    message(STATUS "CoreMach bridge: disabled (external/coremach not checked out)")
    return()
endif()

# CoreMach's engine/version.h is configured from its project version.
file(STRINGS "${COREMACH_SOURCE_DIR}/CMakeLists.txt" _coremach_version_line
     REGEX "^set\\(BLUMACH_PORTABLE_VERSION \"[^\"]+\"\\)")
string(REGEX REPLACE "^set\\(BLUMACH_PORTABLE_VERSION \"([^\"]+)\"\\)$" "\\1"
       COREMACH_VERSION "${_coremach_version_line}")
if(NOT COREMACH_VERSION)
    message(FATAL_ERROR "Unable to read the CoreMach version from external/coremach/CMakeLists.txt")
endif()

function(blumach_add_coremach)
    # Variables set here are inherited by the added directories only.
    set(BUILD_TESTING OFF)
    set(BLUMACH_PORTABLE_VERSION "${COREMACH_VERSION}")
    foreach(_directory IN ITEMS
            engine
            runtime
            components/buses
            components/storage
            components/cpu/i8085
            components/pc
            components/video
            systems/olivetti-m10)
        add_subdirectory("${COREMACH_SOURCE_DIR}/${_directory}"
                         "${CMAKE_BINARY_DIR}/coremach/${_directory}" EXCLUDE_FROM_ALL)
    endforeach()
endfunction()

blumach_add_coremach()

# Build the systems BluMach can run, and only their dependencies.
add_library(blumach_coremach_systems INTERFACE)
target_link_libraries(blumach_coremach_systems INTERFACE
    blumach_runtime
    blumach_system_olivetti_m10
)
add_custom_target(coremach_systems ALL)
add_dependencies(coremach_systems blumach_system_olivetti_m10)

set(BLUMACH_COREMACH_ENABLED ON)
message(STATUS "CoreMach bridge: enabled (CoreMach ${COREMACH_VERSION})")
