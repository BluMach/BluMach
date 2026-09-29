# Standalone catalogue UI regression tests; no emulator, ROMs or user settings.
cmake_minimum_required(VERSION 3.24)
project(BluMachCollectionTests LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test LinguistTools)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(BLUMACH_ROOT "../.." ABSOLUTE)
set(QT_SOURCE "${BLUMACH_ROOT}/src/qt")
file(GLOB_RECURSE CATALOG_INPUTS CONFIGURE_DEPENDS "${QT_SOURCE}/catalog/source/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CATALOG_INPUTS}
    "${BLUMACH_ROOT}/tools/catalog_builder.py" "${BLUMACH_ROOT}/tools/catalog_audit.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${BLUMACH_ROOT}/tools/catalog_builder.py"
    build --source "${QT_SOURCE}/catalog/source" --output "${CMAKE_CURRENT_BINARY_DIR}/catalog"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(collection-ui collection_ui_test.cpp
    "${QT_SOURCE}/qt_blumach_collection.cpp"
    "${QT_SOURCE}/qt_blumach_catalog.cpp"
    "${QT_SOURCE}/qt_blumach_skin.cpp"
    "${QT_SOURCE}/qt_blumach_formfactoricon.cpp"
    "${QT_SOURCE}/qt_vmmanager_config.hpp"
    "${QT_SOURCE}/qdarkstyle/dark/darkstyle.qrc"
    "${QT_SOURCE}/catalog/blumach_marks.qrc"
    "${CMAKE_CURRENT_BINARY_DIR}/catalog/blumach_catalog.qrc")
target_include_directories(collection-ui PRIVATE "${QT_SOURCE}")
target_link_libraries(collection-ui PRIVATE Qt6::Widgets Qt6::Test)
foreach(locale es-ES fr-FR it-IT pt-PT)
    set(qm "${CMAKE_CURRENT_BINARY_DIR}/blumach_${locale}.qm")
    add_custom_command(OUTPUT "${qm}" COMMAND Qt6::lconvert
        -i "${QT_SOURCE}/languages/${locale}.po" -o "${qm}"
        DEPENDS "${QT_SOURCE}/languages/${locale}.po")
    target_sources(collection-ui PRIVATE "${qm}")
endforeach()
add_test(NAME collection-ui COMMAND collection-ui -o -,txt -o collection-ui-results.txt,txt)
set_tests_properties(collection-ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
