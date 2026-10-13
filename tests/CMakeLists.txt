# BluMach host-side contract tests. These tests exercise deterministic emulator
# logic with bounded test doubles and do not require firmware or guest media.

function(blumach_add_host_contract target source)
    add_executable(${target} ${source} ${ARGN})
    target_compile_features(${target} PRIVATE c_std_11)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/include"
        "${PROJECT_SOURCE_DIR}/src/cpu"
        "${PROJECT_SOURCE_DIR}/src/codegen"
        "${PROJECT_SOURCE_DIR}/src/codegen_new"
        "${PROJECT_BINARY_DIR}/src/include"
    )
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_compile_definitions(${target} PRIVATE
            _FILE_OFFSET_BITS=64
            _LARGEFILE_SOURCE=1
            _LARGEFILE64_SOURCE=1
        )
    endif()

    # The contracts use assert() as their test oracle, including in Release
    # configurations where the application itself defines NDEBUG.
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()

    add_test(NAME "host-contract.${target}" COMMAND ${target})
    set_tests_properties("host-contract.${target}" PROPERTIES TIMEOUT 30)
endfunction()

function(blumach_enable_dead_code_elimination target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /Gy)
        target_link_options(${target} PRIVATE /OPT:REF)
    else()
        target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
        if(APPLE)
            target_link_options(${target} PRIVATE "LINKER:-dead_strip")
        else()
            target_link_options(${target} PRIVATE "LINKER:--gc-sections")
        endif()
    endif()
endfunction()

blumach_add_host_contract(machine-runtime-controls machine_runtime_controls_test.c)
blumach_add_host_contract(vx0-nmi vx0_nmi_test.c)
blumach_add_host_contract(vx0-nmi-legacy vx0_nmi_test.c)
target_compile_definitions(vx0-nmi-legacy PRIVATE OLD_NMI_BEHAVIOR)

# Isolate the real legacy operand reader without duplicating its implementation
# or linking the complete emulator. Fail configuration if its boundary changes.
set(vx0_source "${PROJECT_SOURCE_DIR}/src/cpu/vx0.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${vx0_source}")
file(READ "${vx0_source}" vx0_text)
string(REPLACE "\r\n" "\n" vx0_text "${vx0_text}")
string(FIND "${vx0_text}" "static uint64_t\ngeteaq(void)\n" vx0_qword_start)
string(FIND "${vx0_text}" "\nstatic void\nread_ea(" vx0_qword_end)
if(vx0_qword_start LESS 0 OR vx0_qword_end LESS vx0_qword_start)
    message(FATAL_ERROR "Cannot isolate V30 qword reader for its regression test")
endif()
math(EXPR vx0_qword_length "${vx0_qword_end} - ${vx0_qword_start}")
string(SUBSTRING "${vx0_text}" ${vx0_qword_start} ${vx0_qword_length} vx0_qword_reader)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vx0_geteaq.inc" "${vx0_qword_reader}")
blumach_add_host_contract(vx0-fpu-memory vx0_fpu_memory_test.c)
target_include_directories(vx0-fpu-memory PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

set(vx0_biu_source "${PROJECT_SOURCE_DIR}/src/cpu/vx0_biu.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${vx0_biu_source}")
file(READ "${vx0_biu_source}" vx0_biu_text)
string(REPLACE "\r\n" "\n" vx0_biu_text "${vx0_biu_text}")
string(FIND "${vx0_biu_text}" "void\nwait_vx0(" vx0_wait_start)
string(FIND "${vx0_biu_text}" "\n/* This is for external subtraction of cycles" vx0_wait_end)
if(vx0_wait_start LESS 0 OR vx0_wait_end LESS vx0_wait_start)
    message(FATAL_ERROR "Cannot isolate V30 BIU wait for its regression test")
endif()
math(EXPR vx0_wait_length "${vx0_wait_end} - ${vx0_wait_start}")
string(SUBSTRING "${vx0_biu_text}" ${vx0_wait_start} ${vx0_wait_length} vx0_wait_reader)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vx0_wait.inc" "${vx0_wait_reader}")
blumach_add_host_contract(vx0-wait vx0_wait_test.c)
target_include_directories(vx0-wait PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
blumach_add_host_contract(pcs86-speed pcs86_speed_test.c
    "${PROJECT_SOURCE_DIR}/src/cpu/vx0_clock.c")
blumach_enable_dead_code_elimination(pcs86-speed)
if(NOT MSVC)
    # The included legacy board header has an unrelated qualified int return.
    target_compile_options(pcs86-speed PRIVATE -Wno-ignored-qualifiers)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # MinGW retains public entry points from an included board implementation.
    # Internalize this harness only, so unrelated constructors are eliminated;
    # the real register handlers below remain exercised by main().
    set_source_files_properties(pcs86_speed_test.c PROPERTIES
        COMPILE_OPTIONS "-fwhole-program")
endif()
blumach_add_host_contract(m15-lcd-palette m15_lcd_palette_test.c)
target_sources(m15-lcd-palette PRIVATE "${PROJECT_SOURCE_DIR}/src/video/video_panel.c")
blumach_add_host_contract(machine-sound-policy machine_sound_policy_test.c "${PROJECT_SOURCE_DIR}/src/machine/machine_sound_policy.c")
blumach_add_host_contract(video-panel video_panel_test.c "${PROJECT_SOURCE_DIR}/src/video/video_panel.c")
blumach_add_host_contract(selected-firmware selected_firmware_test.c "${PROJECT_SOURCE_DIR}/src/device_bios.c")
blumach_add_host_contract(toshiba-aform-slot toshiba_aform_slot_test.c)
blumach_add_host_contract(t5100-platform t5100_platform_test.c)
target_sources(t5100-platform PRIVATE "${PROJECT_SOURCE_DIR}/src/video/video_panel.c")
blumach_enable_dead_code_elimination(t5100-platform)

blumach_add_host_contract(t5200-display-hotkey t5200_display_hotkey_test.c)
if(NOT MSVC)
    target_compile_options(t5200-display-hotkey PRIVATE -Wno-ignored-qualifiers)
endif()

blumach_add_host_contract(t3200-mfm t3200_mfm_test.c)
blumach_enable_dead_code_elimination(t3200-mfm)
if(NOT MSVC)
    target_compile_options(t3200-mfm PRIVATE -Wno-empty-body)
endif()

blumach_add_host_contract(m15plus-hdu m15plus_hdu_test.c)
blumach_enable_dead_code_elimination(m15plus-hdu)
if(NOT MSVC)
    # Including the existing XTA implementation also parses its unused legacy
    # constructors and logging macros; keep their warning policy unchanged.
    target_compile_options(m15plus-hdu PRIVATE -Wno-empty-body -Wno-implicit-fallthrough)
endif()

blumach_add_host_contract(t3200-ags
    t3200_ags_test.c
    "${PROJECT_SOURCE_DIR}/src/io.c"
)
blumach_enable_dead_code_elimination(t3200-ags)
target_sources(t3200-ags PRIVATE "${PROJECT_SOURCE_DIR}/src/video/video_panel.c")
