# A configuration with a [CoreMach] section must never start the 86Box core
# or be saved by it. Runs BluMach on such configurations, without firmware
# or dialogs, and checks the exit status, the reported reason and that the
# machine folder is left byte-for-byte unchanged.
#
# cmake -DBLUMACH=<executable> -DWORK_DIR=<dir> -DCOREMACH_BUILT=<ON|OFF> -P coremach_startup_guard.cmake

foreach(variable BLUMACH WORK_DIR COREMACH_BUILT)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/roms")
set(ENV{BLUMACH_UNATTENDED} 1)
set(ENV{QT_QPA_PLATFORM} offscreen)

function(snapshot directory out)
    file(GLOB_RECURSE files LIST_DIRECTORIES true RELATIVE "${directory}" "${directory}/*")
    list(SORT files)
    set(state "")
    foreach(file IN LISTS files)
        if(IS_DIRECTORY "${directory}/${file}")
            string(APPEND state "${file}/\n")
        else()
            file(SHA256 "${directory}/${file}" hash)
            string(APPEND state "${file} ${hash}\n")
        endif()
    endforeach()
    set(${out} "${state}" PARENT_SCOPE)
endfunction()

# name: case label; config: blumach.cfg text; expected_*: when built and not.
function(check name config expected_built expected_unbuilt)
    set(vm "${WORK_DIR}/${name}")
    file(MAKE_DIRECTORY "${vm}")
    file(WRITE "${vm}/blumach.cfg" "${config}")
    snapshot("${vm}" before)
    execute_process(
        COMMAND "${BLUMACH}" --vmpath "${vm}" --rompath "${WORK_DIR}/roms"
                --global "${WORK_DIR}/global.cfg" --noconfirm ${ARGN}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE  output
        TIMEOUT 60)
    snapshot("${vm}" after)

    if(COREMACH_BUILT)
        set(expected "${expected_built}")
    else()
        set(expected "${expected_unbuilt}")
    endif()
    list(GET expected 0 expected_status)
    list(GET expected 1 expected_code)
    if(NOT "${status}" STREQUAL "${expected_status}")
        message(FATAL_ERROR "${name}: exit status ${status}, expected ${expected_status}\n${output}")
    endif()
    if(NOT output MATCHES "BluMach: CoreMach (${expected_code}): ")
        message(FATAL_ERROR "${name}: missing reason '${expected_code}'\n${output}")
    endif()
    set(reason "${CMAKE_MATCH_1}")
    if(NOT before STREQUAL after)
        message(FATAL_ERROR "${name}: the machine folder changed\nbefore:\n${before}after:\n${after}")
    endif()
    message(STATUS "${name}: exit ${status}, ${reason}, folder unchanged")
endfunction()

set(m10 "[CoreMach]\nsystem = olivetti-m10\nram_kib = 32\nlanguage = 2\nrtc_time = 1\n")
check(unknown-system "[CoreMach]\nsystem = olivetti-m99\n" "6;unknown_system" "6;not_built")
check(missing-system "[CoreMach]\nram_kib = 32\n\n[Machine]\nmachine = ibmxt\n" "6;missing_system" "6;not_built")
# No firmware is available, so a known system cannot run either.
check(known-system "${m10}" "6;[a-z_]+" "6;not_built")
check(settings "${m10}" "0;settings_unavailable" "6;not_built" --settings)

file(REMOVE_RECURSE "${WORK_DIR}")
