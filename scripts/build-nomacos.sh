#!/usr/bin/env bash
#
# Build for Linux and Windows only and produce the final X-Plane plugin directory.
#
# Usage (inside Docker):
#   scripts/build-nomacos.sh <PluginName>
#
# Output:
#   build/docker/deploy/<PluginName>/
#   ├── lin_x64/<PluginName>.xpl + libvosk.so
#   ├── win_x64/<PluginName>.xpl + libvosk.dll + runtime DLLs
#   └── model/

set -euo pipefail

PLUGIN_NAME="${1:?Usage: $0 <PluginName>}"
PRESETS=(linux-x64 windows-x64)

echo "=== Configuring and building all presets ==="
for preset in "${PRESETS[@]}"; do
  echo "--- ${preset} ---"
  cmake --preset "$preset"
  cmake --build --preset "$preset"
done

DEPLOY_DIR=build/docker/deploy

echo "=== Assembling final plugin directory ==="
rm -rf "${DEPLOY_DIR}"
mkdir -p "${DEPLOY_DIR}/${PLUGIN_NAME}/lin_x64" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64"

cp "build/docker/linux-x64/${PLUGIN_NAME}/lin_x64/${PLUGIN_NAME}.xpl" "${DEPLOY_DIR}/${PLUGIN_NAME}/lin_x64/"
cp "build/docker/windows-x64/${PLUGIN_NAME}/win_x64/${PLUGIN_NAME}.xpl" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64/"

echo "=== Copying vosk libraries ==="
cp "build/docker/linux-x64/_deps/vosk-src/libvosk.so" "${DEPLOY_DIR}/${PLUGIN_NAME}/lin_x64/"
cp "build/docker/windows-x64/_deps/vosk-src/libvosk.dll" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64/"
cp "build/docker/windows-x64/_deps/vosk-src/libstdc++-6.dll" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64/"
cp "build/docker/windows-x64/_deps/vosk-src/libwinpthread-1.dll" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64/"
cp "build/docker/windows-x64/_deps/vosk-src/libgcc_s_seh-1.dll" "${DEPLOY_DIR}/${PLUGIN_NAME}/win_x64/"

echo "=== Copying vosk model ==="
mkdir -p "${DEPLOY_DIR}/${PLUGIN_NAME}/model"
cp -r "build/docker/linux-x64/_deps/vosk_model-src/." "${DEPLOY_DIR}/${PLUGIN_NAME}/model/"

echo "=== Creating ${PLUGIN_NAME}.zip ==="
(cd "${DEPLOY_DIR}" && zip -r "${PLUGIN_NAME}.zip" "${PLUGIN_NAME}/")

echo "=== Done ==="
echo "Plugin directory:"
find "${DEPLOY_DIR}/${PLUGIN_NAME}" -type f | sort
file "${DEPLOY_DIR}/${PLUGIN_NAME}"/*/"${PLUGIN_NAME}.xpl" 2>/dev/null || true
echo "Archive: ${DEPLOY_DIR}/${PLUGIN_NAME}.zip ($(du -h "${DEPLOY_DIR}/${PLUGIN_NAME}.zip" | cut -f1))"
