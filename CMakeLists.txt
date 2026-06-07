cmake_minimum_required(VERSION 3.25)

set(PLUGIN_NAME "VControl")
set(PLUGIN_VERSION "0.1.0")
set(PLUGIN_SIGNATURE "com.${PLUGIN_NAME}")
set(PLUGIN_DESC "${PLUGIN_NAME} v${PLUGIN_VERSION}")
project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

set(LOG_LEVEL "Debug" CACHE STRING "Log level")
set(VOSK_VERBOSE 0 CACHE STRING "Enable Vosk internal logging (0=off, 1=on)")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS Debug Info Warning Error)

string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)
set(DOCKER_IMAGE "${PROJECT_NAME_LOWER}-build")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile_commands.json for editor tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# X-Plane SDK 4.3.0
# ---------------------------------------------------------------------------
FetchContent_Declare(
    xpsdk
    URL      https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK430.zip
    URL_HASH SHA256=b9875ab27b593927b4f9b3e0ddfffe7401ee5dce6d86b50aea0da65f70ff7816
)
FetchContent_MakeAvailable(xpsdk)

set(XPSDK_ROOT "${xpsdk_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# vosk 0.3.45
# ---------------------------------------------------------------------------
if(WIN32)
  set(VOSK_URL "https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip")
  set(VOSK_URL_HASH "SHA256=f1dcc9cca460630f81ea8f71794f69c80bed6556d2a4e6237b5785e1d2dff34b")
  set(VOSK_LIB_NAME "libvosk.dll")
  set(VOSK_RUNTIME_DEPS libstdc++-6.dll libwinpthread-1.dll libgcc_s_seh-1.dll)
else()
  set(VOSK_URL "https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-linux-x86_64-0.3.45.zip")
  set(VOSK_URL_HASH "SHA256=bbdc8ed85c43979f6443142889770ea95cbfbc56cffb5c5dcd73afa875c5fbb2")
  set(VOSK_LIB_NAME "libvosk.so")
  set(VOSK_RUNTIME_DEPS "")
endif()

FetchContent_Declare(
  vosk
  URL      ${VOSK_URL}
  URL_HASH ${VOSK_URL_HASH}
)
FetchContent_MakeAvailable(vosk)

# The upstream libvosk.so ships without a SONAME. Without one, the linker
# records the build-relative path in NEEDED (e.g. "_deps/vosk-src/libvosk.so")
# which breaks runtime loading via RUNPATH ($ORIGIN). Set it once after fetch.
if(NOT WIN32)
  execute_process(
    COMMAND patchelf --set-soname libvosk.so "${vosk_SOURCE_DIR}/libvosk.so"
  )
endif()

# ---------------------------------------------------------------------------
# vosk small model 0.15
# ---------------------------------------------------------------------------
FetchContent_Declare(
  vosk_model
  URL https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
  URL_HASH SHA256=30f26242c4eb449f948e42cb302dd7a686cb29a3423a8367f99ff41780942498
)
FetchContent_MakeAvailable(vosk_model)

# ---------------------------------------------------------------------------
# miniaduio 0.11.25
# ---------------------------------------------------------------------------
FetchContent_Declare(
  miniaudio
  URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
  URL_HASH SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
)
FetchContent_MakeAvailable(miniaudio)

# ---------------------------------------------------------------------------
# fmt 12.1.0
# ---------------------------------------------------------------------------
FetchContent_Declare(
  fmt
  URL https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
  URL_HASH SHA256=ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea
)
FetchContent_MakeAvailable(fmt)

# ---------------------------------------------------------------------------
# nlohmann/json 3.11.3
# ---------------------------------------------------------------------------
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)


# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------

set(SRC_FILES
  src/main.cpp
  src/logger.cpp
  src/recorder.cpp
  src/recognizer.cpp
  src/controldatarefs.cpp
)

configure_file(
    src/config.h.in
    "${PROJECT_BINARY_DIR}/generated/config.h"
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${PROJECT_BINARY_DIR}/generated
    ${XPSDK_ROOT}/CHeaders/XPLM
    ${XPSDK_ROOT}/CHeaders/Widgets
    ${CMAKE_BINARY_DIR}
    ${miniaudio_SOURCE_DIR}
    ${vosk_SOURCE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)

# Copy vosk shared library next to the .xpl so the loader finds it
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${vosk_SOURCE_DIR}/${VOSK_LIB_NAME}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${VOSK_LIB_NAME}"
)

# Copy vosk runtime dependencies (Windows MinGW DLLs; empty on Linux)
foreach(_dll ${VOSK_RUNTIME_DEPS})
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${vosk_SOURCE_DIR}/${_dll}"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${_dll}"
  )
endforeach()

# Symlink the Vosk model directory next to the executable.
# Use a relative target so the link resolves correctly both inside Docker
# and on the host (an absolute path would point into the container's /src mount).
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    "../../_deps/vosk_model-src"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/model"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  fmt::fmt
  nlohmann_json::nlohmann_json
)

# Link vosk: on Linux use -l so NEEDED records "libvosk.so" not a full path;
# on Windows link the import library directly.
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE "${vosk_SOURCE_DIR}/libvosk.lib")
else()
  target_link_options(${PROJECT_NAME} PRIVATE -L${vosk_SOURCE_DIR} -lvosk)
endif()


# X-Plane SDK version defines (XP11+ compatibility).
target_compile_definitions(${PROJECT_NAME} PRIVATE
    XPLM200
    XPLM210
    XPLM300
    XPLM301
    ${PROJECT_NAME}_VERSION="${PROJECT_VERSION}"
)

# Prevent the compiler from registering destructors for static C++ objects
# via __cxa_atexit. When X-Plane unloads the plugin (dlclose), those
# destructors can reference code/data that has already been unmapped,
# causing a crash on shutdown or plugin reload.
target_compile_options(${PROJECT_NAME} PRIVATE -fno-c++-static-destructors)

# Mark the shared library as non-deletable so dlclose() skips __cxa_finalize.
# This is needed because linked libraries may register their own atexit
# destructors that we can't control with compile flags.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-z,nodelete")
endif()

# ---------------------------------------------------------------------------
# Platform-specific configuration
# ---------------------------------------------------------------------------
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IBM=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${XPSDK_ROOT}/Libraries/Win/XPLM_64.lib
        ${XPSDK_ROOT}/Libraries/Win/XPWidgets_64.lib
        opengl32
    )
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE APL=1)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-F${XPSDK_ROOT}/Libraries/Mac"
        "-framework XPLM"
        "-framework XPWidgets"
    )
else()
    # Linux: symbols resolved at runtime by X-Plane.
    target_compile_definitions(${PROJECT_NAME} PRIVATE LIN=1)
    target_link_options(${PROJECT_NAME} PRIVATE -nodefaultlibs)
endif()

# Output as .xpl regardless of platform.
# Place the binary in the X-Plane plugin directory layout:
#   ${PROJECT_NAME}/{lin_x64,win_x64,mac_x64}/${PROJECT_NAME}.xpl
if(WIN32)
    set(PLUGIN_PLATFORM_DIR "win_x64")
elseif(APPLE)
    set(PLUGIN_PLATFORM_DIR "mac_x64")
else()
    set(PLUGIN_PLATFORM_DIR "lin_x64")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/${PLUGIN_PLATFORM_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/${PLUGIN_PLATFORM_DIR}"
)

# ---------------------------------------------------------------------------
# Custom targets for dev workflow: build -> deploy -> install-plugin
#
# Local builds use Docker with zig c++ (targeting linux-x64 only) so that
# dev builds match the deployment compiler and ABI.
# ---------------------------------------------------------------------------
set(DOCKER_BUILD_XPL "${CMAKE_SOURCE_DIR}/build/docker/linux-x64/${PROJECT_NAME}/lin_x64/${PROJECT_NAME}.xpl")
set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy/${PROJECT_NAME}/lin_x64")
set(XPLANE_PLUGIN_DIR "" CACHE PATH "Path to X-Plane plugins directory (e.g. .../X-Plane 12/Resources/plugins)")

# build: compile the .xpl for linux-x64 via Docker (zig c++)
add_custom_target(build
    COMMAND docker build -q -t ${DOCKER_IMAGE} "${CMAKE_SOURCE_DIR}"
    COMMAND docker run --rm -v "${CMAKE_SOURCE_DIR}:/src" ${DOCKER_IMAGE} cmake --preset linux-x64
    COMMAND docker run --rm -v "${CMAKE_SOURCE_DIR}:/src" ${DOCKER_IMAGE} cmake --build --preset linux-x64
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building ${PROJECT_NAME} linux-x64 via Docker (zig c++)"
    USES_TERMINAL
)

# deploy: assemble plugin directory layout under build/deploy/
set(DOCKER_BUILD_DEPS "${CMAKE_SOURCE_DIR}/build/docker/linux-x64/_deps")

add_custom_target(deploy
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${DOCKER_BUILD_XPL}" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${DOCKER_BUILD_DEPS}/vosk-src/libvosk.so" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${DOCKER_BUILD_DEPS}/vosk_model-src" "${CMAKE_BINARY_DIR}/deploy/${PROJECT_NAME}/model"
    COMMENT "Deploying ${PROJECT_NAME} to ${DEPLOY_DIR}"
)
add_dependencies(deploy build)

# install-plugin: copy deployed plugin to X-Plane plugins directory
if(XPLANE_PLUGIN_DIR)
    add_custom_target(install-plugin
        COMMAND ${CMAKE_COMMAND} -E make_directory "${XPLANE_PLUGIN_DIR}/${PROJECT_NAME}/lin_x64"
        COMMAND ${CMAKE_COMMAND} -E copy "${DEPLOY_DIR}/${PROJECT_NAME}.xpl" "${XPLANE_PLUGIN_DIR}/${PROJECT_NAME}/lin_x64/"
        COMMAND ${CMAKE_COMMAND} -E copy "${DEPLOY_DIR}/libvosk.so" "${XPLANE_PLUGIN_DIR}/${PROJECT_NAME}/lin_x64/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/deploy/${PROJECT_NAME}/model" "${XPLANE_PLUGIN_DIR}/${PROJECT_NAME}/model"
        COMMENT "Installing ${PROJECT_NAME} to ${XPLANE_PLUGIN_DIR}/${PROJECT_NAME}"
    )
else()
    add_custom_target(install-plugin
        COMMAND ${CMAKE_COMMAND} -E echo "ERROR: XPLANE_PLUGIN_DIR is not set. Set it in CMakeUserPresets.json or pass -DXPLANE_PLUGIN_DIR=..."
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()
add_dependencies(install-plugin deploy)

# docker-deploy: cross-compile via Docker and produce release zip
add_custom_target(docker-deploy
    COMMAND docker build -t ${DOCKER_IMAGE} "${CMAKE_SOURCE_DIR}"
    COMMAND docker run --rm -v "${CMAKE_SOURCE_DIR}:/src" ${DOCKER_IMAGE} scripts/build-nomacos.sh ${PROJECT_NAME}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building release for Linux and Windows via Docker"
    USES_TERMINAL
)

# docker-rebuild: rebuild the Docker build image
add_custom_target(docker-rebuild
    COMMAND docker build -t ${DOCKER_IMAGE} "${CMAKE_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Rebuilding Docker image ${DOCKER_IMAGE} from Dockerfile"
    USES_TERMINAL
)

# docker-clean-image: remove the Docker build image
add_custom_target(docker-clean-image
    COMMAND docker rmi ${DOCKER_IMAGE}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Removing Docker image ${DOCKER_IMAGE}"
)
