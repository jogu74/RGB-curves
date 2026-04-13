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
set(QT_VERSION_HINT "6.8.3")
set(QT_LOCAL_ROOT "")
set(OBS_APP_FRAMEWORK_DIR "")

if(WIN32)
  set(QT_LOCAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/${QT_VERSION_HINT}/msvc2022_64")
elseif(APPLE)
  set(QT_LOCAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/${QT_VERSION_HINT}/macos")
  set(OBS_APP_FRAMEWORK_DIR "/Applications/OBS.app/Contents/Frameworks")
endif()

if(EXISTS "${QT_LOCAL_ROOT}/lib/cmake/Qt6")
  list(PREPEND CMAKE_PREFIX_PATH "${QT_LOCAL_ROOT}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
  find_package(OpenGL REQUIRED)
  set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES OpenGL::GL)
endif()

find_path(LIBOBS_INCLUDE_DIR
  NAMES obs-module.h
  HINTS ENV OBS_INCLUDE_DIR
        "${OBS_LOCAL_SOURCE_DIR}/libobs"
  PATH_SUFFIXES libobs obs Headers
)

find_path(LIBOBS_FRAMEWORK_HEADERS
  NAMES obs-module.h
  HINTS
    "${OBS_APP_FRAMEWORK_DIR}/libobs.framework/Headers"
    ENV OBS_FRAMEWORK_HEADERS
  PATH_SUFFIXES libobs obs
)

find_path(LIBOBS_ROOT_INCLUDE_DIR
  NAMES util/base.h
  HINTS
    "${OBS_LOCAL_SOURCE_DIR}/libobs"
    ENV OBS_ROOT_INCLUDE_DIR
    "${LIBOBS_FRAMEWORK_HEADERS}"
)

find_library(LIBOBS_LIBRARY
  NAMES obs libobs
  HINTS ENV OBS_LIB_DIR
        "${OBS_LOCAL_SDK_DIR}"
        "${OBS_APP_FRAMEWORK_DIR}"
)

if(APPLE AND EXISTS "${OBS_APP_FRAMEWORK_DIR}/libobs.framework/Versions/A/libobs")
  set(LIBOBS_LIBRARY "${OBS_APP_FRAMEWORK_DIR}/libobs.framework/Versions/A/libobs" CACHE FILEPATH "libobs binary" FORCE)
endif()

if(NOT LIBOBS_INCLUDE_DIR AND LIBOBS_FRAMEWORK_HEADERS)
  set(LIBOBS_INCLUDE_DIR "${LIBOBS_FRAMEWORK_HEADERS}")
endif()

if(NOT LIBOBS_ROOT_INCLUDE_DIR AND LIBOBS_FRAMEWORK_HEADERS)
  set(LIBOBS_ROOT_INCLUDE_DIR "${LIBOBS_FRAMEWORK_HEADERS}")
endif()

if(NOT LIBOBS_INCLUDE_DIR OR NOT LIBOBS_ROOT_INCLUDE_DIR OR NOT LIBOBS_LIBRARY)
  message(FATAL_ERROR
    "Could not find libobs. Set OBS_INCLUDE_DIR, OBS_ROOT_INCLUDE_DIR and OBS_LIB_DIR to your OBS SDK/include and library paths. On macOS you can also point OBS_FRAMEWORK_HEADERS at libobs.framework/Headers.")
endif()

if(NOT TARGET obs-libobs)
  add_library(obs-libobs UNKNOWN IMPORTED)
  set_target_properties(obs-libobs PROPERTIES
    IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
  )
endif()

set(OBS_EXTRA_INCLUDE_DIRS "")
if(EXISTS "${OBS_LOCAL_SOURCE_DIR}/deps/simde")
  list(APPEND OBS_EXTRA_INCLUDE_DIRS "${OBS_LOCAL_SOURCE_DIR}/deps/simde")
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
  ${OBS_EXTRA_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obs-rgb-curves PRIVATE
  obs-libobs
  Qt6::Widgets
)

if(WIN32)
  target_compile_definitions(obs-rgb-curves PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
endif()

if(MSVC)
  target_compile_options(obs-rgb-curves PRIVATE /W4 /permissive-)
else()
  target_compile_options(obs-rgb-curves PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(APPLE AND EXISTS "${QT_LOCAL_ROOT}/lib")
  # Prefer the vendored Qt frameworks over the copies bundled inside OBS.app.
  target_compile_options(obs-rgb-curves PRIVATE "-F${QT_LOCAL_ROOT}/lib")
  target_link_options(obs-rgb-curves PRIVATE "-F${QT_LOCAL_ROOT}/lib")
endif()

set_target_properties(obs-rgb-curves PROPERTIES
  PREFIX ""
  OUTPUT_NAME "obs-rgb-curves"
)

if(APPLE)
  set_target_properties(obs-rgb-curves PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "plugin"
  )
endif()

if(OBS_RGB_CURVES_COPY_DATA)
  if(APPLE)
    add_custom_command(TARGET obs-rgb-curves POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:obs-rgb-curves>/Resources/effects"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/effects/rgb-curves.effect"
        "$<TARGET_BUNDLE_CONTENT_DIR:obs-rgb-curves>/Resources/effects/rgb-curves.effect"
    )
  else()
    add_custom_command(TARGET obs-rgb-curves POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:obs-rgb-curves>/data/obs-rgb-curves/effects"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/effects/rgb-curves.effect"
        "$<TARGET_FILE_DIR:obs-rgb-curves>/data/obs-rgb-curves/effects/rgb-curves.effect"
    )
  endif()
endif()

if(APPLE)
  set(OBS_MAC_PLUGIN_INSTALL_DIR "$ENV{HOME}/Library/Application Support/obs-studio/plugins")
  install(TARGETS obs-rgb-curves
    LIBRARY DESTINATION "${OBS_MAC_PLUGIN_INSTALL_DIR}"
  )
else()
  install(TARGETS obs-rgb-curves
    LIBRARY DESTINATION obs-plugins/64bit
    RUNTIME DESTINATION obs-plugins/64bit
  )
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/effects/rgb-curves.effect"
    DESTINATION data/obs-plugins/obs-rgb-curves/effects
  )
endif()
