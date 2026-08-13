# LimbVigor — RE_Kenshi plugin
# Linux / IDE: stubs (edit only).
# Windows MSVC + GitHub Actions: real KenshiLib → LimbVigor.dll
#
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
#     -DKENSHILIB_DIR=.../KenshiLib \
#     -DBOOST_INCLUDE_DIR=.../boost_1_60_0 \
#     -DLIMBVIGOR_FORCE_GAME_BUILD=ON
#   cmake --build build --config Release

cmake_minimum_required(VERSION 3.20)
project(LimbVigor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIMBVIGOR_FORCE_GAME_BUILD
  "Build against real KenshiLib (Windows MSVC)." OFF)
option(LIMBVIGOR_BUILD_TESTS "Build the headless sim test" ON)

set(LV_SRC
  src/LimbVigor.cpp
  src/lv_config.cpp
  src/lv_sim.cpp
  src/lv_persist.cpp
  src/lv_game.cpp
  src/lv_hooks.cpp
)

add_library(LimbVigor SHARED ${LV_SRC})

set(_GAME FALSE)
if(MSVC OR LIMBVIGOR_FORCE_GAME_BUILD)
  set(_GAME TRUE)
endif()

if(DEFINED ENV{KENSHILIB_DIR} AND NOT DEFINED KENSHILIB_DIR)
  set(KENSHILIB_DIR "$ENV{KENSHILIB_DIR}")
endif()
if(DEFINED ENV{BOOST_INCLUDE_DIR} AND NOT DEFINED BOOST_INCLUDE_DIR)
  set(BOOST_INCLUDE_DIR "$ENV{BOOST_INCLUDE_DIR}")
endif()

if(_GAME)
  if(NOT KENSHILIB_DIR OR NOT EXISTS "${KENSHILIB_DIR}/Include/Debug.h")
    message(FATAL_ERROR "KENSHILIB_DIR missing Include/Debug.h (got '${KENSHILIB_DIR}')")
  endif()

  target_include_directories(LimbVigor PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${KENSHILIB_DIR}/Include"
  )
  if(BOOST_INCLUDE_DIR AND EXISTS "${BOOST_INCLUDE_DIR}/boost")
    target_include_directories(LimbVigor PRIVATE "${BOOST_INCLUDE_DIR}")
  endif()

  target_compile_definitions(LimbVigor PRIVATE
    _WINDOWS _USRDLL UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Release>:NDEBUG>
  )

  if(MSVC)
    set_property(TARGET LimbVigor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    # /GL required so KenshiLib::GetRealAddress can see member pointers
    target_compile_options(LimbVigor PRIVATE /W3 /EHsc /bigobj /GL /O2)
    target_link_options(LimbVigor PRIVATE /LTCG /INCREMENTAL:NO)
  endif()

  set(KENSHI_LIB_DIR "${KENSHILIB_DIR}/Libraries")
  if(EXISTS "${KENSHI_LIB_DIR}/KenshiLib.lib")
    file(SIZE "${KENSHI_LIB_DIR}/KenshiLib.lib" _libsize)
  else()
    set(_libsize 0)
  endif()
  if(_libsize LESS 10000)
    message(FATAL_ERROR "KenshiLib.lib missing or Git-LFS pointer in ${KENSHI_LIB_DIR}")
  endif()

  target_link_directories(LimbVigor PRIVATE "${KENSHI_LIB_DIR}")
  target_link_libraries(LimbVigor PRIVATE KenshiLib)
  message(STATUS "GAME BUILD: KenshiLib ${_libsize} bytes")
else()
  message(STATUS "IDE MODE: stubs — not a game DLL")
  target_include_directories(LimbVigor PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/stubs"
  )
  target_compile_definitions(LimbVigor PRIVATE LIMBVIGOR_IDE=1)
  if(NOT MSVC)
    target_compile_options(LimbVigor PRIVATE -Wall -Wno-unknown-pragmas)
  endif()
endif()

set_target_properties(LimbVigor PROPERTIES
  OUTPUT_NAME "LimbVigor"
  PREFIX ""
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

if(LIMBVIGOR_BUILD_TESTS)
  add_executable(lv_sim_test src/lv_sim_test.cpp src/lv_config.cpp src/lv_sim.cpp)
  target_include_directories(lv_sim_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_compile_definitions(lv_sim_test PRIVATE LIMBVIGOR_IDE=1)
endif()
