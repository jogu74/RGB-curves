cmake_minimum_required(VERSION 3.22)

project(obs-rgb-curves VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

option(OBS_RGB_CURVES_COPY_DATA "Copy effect files next to the built plugin" ON)

set(OBS_LOCAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/obs-studio")
set(OBS_LOCAL_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/obs-sdk")
set(QT_LOCAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/msvc2022_64")

if(EXISTS "${QT_LOCAL_ROOT}/lib/cmake/Qt6")
  list(PREPEND CMAKE_PREFIX_PATH "${QT_LOCAL_ROOT}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

find_path(LIBOBS_INCLUDE_DIR
  NAMES obs-module.h
  HINTS ENV OBS_INCLUDE_DIR
        "${OBS_LOCAL_SOURCE_DIR}/libobs"
  PATH_SUFFIXES libobs obs
)

find_path(LIBOBS_ROOT_INCLUDE_DIR
  NAMES util/base.h
  HINTS
    "${OBS_LOCAL_SOURCE_DIR}/libobs"
    ENV OBS_ROOT_INCLUDE_DIR
)

find_library(LIBOBS_LIBRARY
  NAMES obs libobs
  HINTS ENV OBS_LIB_DIR
        "${OBS_LOCAL_SDK_DIR}"
)

if(NOT LIBOBS_INCLUDE_DIR OR NOT LIBOBS_ROOT_INCLUDE_DIR OR NOT LIBOBS_LIBRARY)
  message(FATAL_ERROR
    "Could not find libobs. Set OBS_INCLUDE_DIR, OBS_ROOT_INCLUDE_DIR and OBS_LIB_DIR to your OBS SDK/include and library paths.")
endif()

add_library(obs-rgb-curves MODULE
  src/plugin-main.cpp
  src/rgb_curves_filter.cpp
  src/rgb_curves_filter.hpp
  src/curve_editor_dialog.cpp
  src/curve_editor_dialog.hpp
  src/curve_widget.cpp
  src/curve_widget.hpp
  src/curve_types.hpp
)

target_include_directories(obs-rgb-curves PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${LIBOBS_ROOT_INCLUDE_DIR}
  ${LIBOBS_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obs-rgb-curves PRIVATE
  ${LIBOBS_LIBRARY}
  Qt6::Widgets
)

target_compile_definitions(obs-rgb-curves PRIVATE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

if(MSVC)
  target_compile_options(obs-rgb-curves PRIVATE /W4 /permissive-)
else()
  target_compile_options(obs-rgb-curves PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(obs-rgb-curves PROPERTIES
  PREFIX ""
  OUTPUT_NAME "obs-rgb-curves"
)

if(OBS_RGB_CURVES_COPY_DATA)
  add_custom_command(TARGET obs-rgb-curves POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:obs-rgb-curves>/data/obs-rgb-curves/effects"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/data/effects/rgb-curves.effect"
      "$<TARGET_FILE_DIR:obs-rgb-curves>/data/obs-rgb-curves/effects/rgb-curves.effect"
  )
endif()
