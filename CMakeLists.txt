cmake_minimum_required(VERSION 3.16)

project(botyarapix
    VERSION 0.1.0
    DESCRIPTION "Cross-platform monochrome pixel art generator"
    LANGUAGES CXX)

# ------------------------------------------------------------------ стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BOTYARAPIX_STRICT_WARNINGS "Enable -Wall -Wextra -Wpedantic" ON)

# --------------------------------------------------------- сторонние зависимости
set(BOTYARAPIX_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# nlohmann/json: сначала пробуем системный пакет, иначе берём single-header из third_party
find_package(nlohmann_json 3.2 QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "botyarapix: using system nlohmann_json ${nlohmann_json_VERSION}")
    set(BOTYARAPIX_JSON_TARGET nlohmann_json::nlohmann_json)
else()
    if(NOT EXISTS "${BOTYARAPIX_THIRD_PARTY_DIR}/nlohmann/json.hpp")
        message(FATAL_ERROR
            "nlohmann/json.hpp not found. Either install nlohmann-json3-dev "
            "or put the single header at ${BOTYARAPIX_THIRD_PARTY_DIR}/nlohmann/json.hpp")
    endif()
    message(STATUS "botyarapix: using bundled nlohmann_json (third_party/)")
    add_library(bp_nlohmann_json INTERFACE)
    target_include_directories(bp_nlohmann_json SYSTEM INTERFACE "${BOTYARAPIX_THIRD_PARTY_DIR}")
    set(BOTYARAPIX_JSON_TARGET bp_nlohmann_json)
endif()

# stb_image_write: header-only, реализация компилируется внутри ImageExporter.cpp
if(NOT EXISTS "${BOTYARAPIX_THIRD_PARTY_DIR}/stb/stb_image_write.h")
    message(FATAL_ERROR "stb_image_write.h not found at ${BOTYARAPIX_THIRD_PARTY_DIR}/stb/")
endif()
add_library(bp_stb INTERFACE)
target_include_directories(bp_stb SYSTEM INTERFACE "${BOTYARAPIX_THIRD_PARTY_DIR}")

# ------------------------------------------------------------------------ цель
add_executable(botyarapix
    src/main.cpp
    src/app/Application.cpp
    src/core/ConfigManager.cpp
    src/core/ImageExporter.cpp
    src/core/PlatformPaths.cpp
    src/core/Terminal.cpp
    src/core/Translator.cpp
    src/core/Utils.cpp
    src/generators/GeneratorRegistry.cpp
    src/generators/RandomGenerator.cpp
    src/generators/SymmetryGenerator.cpp
    src/generators/CellularGenerator.cpp
)

target_include_directories(botyarapix PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(botyarapix PRIVATE ${BOTYARAPIX_JSON_TARGET} bp_stb)

target_compile_definitions(botyarapix PRIVATE
    BOTYARAPIX_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Debug>:BOTYARAPIX_DEBUG>
)

if(MSVC)
    target_compile_definitions(botyarapix PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(botyarapix PRIVATE /utf-8)
    if(BOTYARAPIX_STRICT_WARNINGS)
        target_compile_options(botyarapix PRIVATE /W4)
    endif()
else()
    if(BOTYARAPIX_STRICT_WARNINGS)
        target_compile_options(botyarapix PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# -------------------------------------------------------------------- установка
include(GNUInstallDirs)
install(TARGETS botyarapix RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES README.md DESTINATION "${CMAKE_INSTALL_DOCDIR}" OPTIONAL)

# --------------------------------------------------------------- ctest (опционально)
enable_testing()
add_test(NAME selftest COMMAND botyarapix --selftest)
add_test(NAME list COMMAND botyarapix --list)

message(STATUS "botyarapix ${PROJECT_VERSION}: build type = ${CMAKE_BUILD_TYPE}")
