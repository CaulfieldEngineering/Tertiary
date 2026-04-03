#!/bin/bash
# =========================================================================
#  Wonderland Audio - Local Build Script (macOS)
#
#  Builds the native plugin (VST3 + Standalone + Installer) and optionally
#  the Wonderlab web version (WASM + web host).
#
#  Prerequisites:
#    - Xcode Command Line Tools, CMake (native build)
#    - Emscripten SDK: set EMSDK env var, or install to ~/emsdk
#    - Wonderlab repo cloned alongside this repo (web build)
# =========================================================================

set -e

# --- Navigate to Project Root ------------------------------------------------
cd "$(dirname "$0")"
cd ..
PROJECT_ROOT="$(pwd)"

echo ""
echo " ========================================================================="
echo "   Wonderland Audio - Local Build"
echo " ========================================================================="
echo ""

# =============================================================================
# STEP 1: Read Project Configuration
# =============================================================================

echo " [1/6] Reading configuration from config.cmake..."
echo ""

declare -A config_vars
while IFS=' ' read -r key value; do
    value=$(echo "$value" | tr -d '"')
    config_vars[$key]=$value
    printf "         %-30s = %s\n" "$key" "$value"
done < <(grep 'set(w' ./config.cmake | sed -e 's/set(//g' -e 's/)//g' -e 's/\s\+/ /g')

REPOSITORY_NAME="${config_vars[wPLUGIN_NAME]}"
FORMATTED_COMPANY_NAME=$(echo "${config_vars[wCOMPANY_NAME]}" | tr -d ' ')
VERSION="${config_vars[wPROJECT_VERSION]}"

echo ""
echo "         Plugin:  ${REPOSITORY_NAME}"
echo "         Version: ${VERSION}"
echo "         Company: ${FORMATTED_COMPANY_NAME}"
echo ""

# =============================================================================
# STEP 2: Build Options
# =============================================================================

echo " [2/6] Build options"
echo ""

while true; do
    read -p "         Build Demo version? (y/n): " BUILD_VERSION
    case $BUILD_VERSION in
        [Yy]* )
            DEMO_OPTION="-DBUILD_DEMO=ON"
            IS_DEMO=true
            echo "         > Demo version selected"
            echo ""
            break;;
        [Nn]* )
            DEMO_OPTION="-DBUILD_DEMO=OFF"
            IS_DEMO=false
            echo "         > Full version selected"
            echo ""
            break;;
        * )
            echo "         Invalid choice. Please enter 'y' or 'n'."
            echo "";;
    esac
done

while true; do
    read -p "         Build Web version?  (y/n): " BUILD_WEB
    case $BUILD_WEB in
        [Yy]* )
            echo "         > Web build will run after native build"
            echo ""
            break;;
        [Nn]* )
            echo "         > Skipping web build"
            echo ""
            break;;
        * )
            echo "         Invalid choice. Please enter 'y' or 'n'."
            echo "";;
    esac
done

# -- Version suffix --
if [ "$IS_DEMO" = true ]; then
    VERSION_SUFFIX="${VERSION}.D"
else
    VERSION_SUFFIX="${VERSION}"
fi

# =============================================================================
# STEP 3: Native Build (VST3 + Standalone)
# =============================================================================

echo " [3/6] Building native plugin..."
echo ""

BUILD_TYPE=Release

# Create build directory
mkdir -p build/output
cd build

# Configure
echo "         Configuring CMake..."
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
cmake -G Xcode -S .. -B . -DCMAKE_BUILD_TYPE=$BUILD_TYPE $DEMO_OPTION
echo "         Configuration complete."
echo ""

# Build
echo "         Compiling..."
cmake --build . --config $BUILD_TYPE
echo "         Build complete."
echo ""

# =============================================================================
# STEP 4: macOS Installer Packages
# =============================================================================

echo " [4/6] Creating macOS installer packages..."
echo ""
echo "         Company: ${FORMATTED_COMPANY_NAME}"
echo "         Version: ${VERSION_SUFFIX}"
echo ""

# -- Standalone package --
echo "         Building Standalone package..."
pkgbuild \
    --root "./plugin/${REPOSITORY_NAME}_artefacts/Release/Standalone" \
    --identifier "com.${FORMATTED_COMPANY_NAME}.${REPOSITORY_NAME}.standalone" \
    --version $VERSION \
    --install-location "/Applications/${REPOSITORY_NAME}" \
    "./output/${REPOSITORY_NAME}_Standalone_${VERSION_SUFFIX}.pkg"
echo "         Standalone package complete."

# -- VST3 package --
echo "         Building VST3 package..."
pkgbuild \
    --root "./plugin/${REPOSITORY_NAME}_artefacts/Release/VST3" \
    --identifier "com.${FORMATTED_COMPANY_NAME}.${REPOSITORY_NAME}.vst3" \
    --version $VERSION \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    "./output/${REPOSITORY_NAME}_VST3_${VERSION_SUFFIX}.pkg"
echo "         VST3 package complete."

# -- Distribution package --
echo "         Building distribution installer..."
cp ../build/distribution.xml ../build/output/distribution.xml
productbuild \
    --distribution "./output/distribution.xml" \
    --package-path "./output" \
    "./output/${REPOSITORY_NAME}MacInstaller_${VERSION_SUFFIX}.pkg"
echo "         Distribution installer complete."
echo ""

# =============================================================================
# STEP 5: Wonderlab Web Build (WASM)
# =============================================================================

# Return to project root
cd "${PROJECT_ROOT}"

if [[ ! "$BUILD_WEB" =~ ^[Yy] ]]; then
    echo ""
    echo " ========================================================================="
    echo "   Build complete!"
    echo " ========================================================================="
    echo ""
    exit 0
fi

echo " [5/6] Building Wonderlab web version..."
echo ""
echo "         Plugin:  ${REPOSITORY_NAME} v${VERSION}"
echo ""

# -- Paths --
WONDERLAB_DIR="${PROJECT_ROOT}/../Wonderlab"
PLUGIN_SRC_DIR="${PROJECT_ROOT}/plugin/source"
WEB_BUILD_DIR="${PROJECT_ROOT}/build/web"
BINARY_DATA_DIR="${PROJECT_ROOT}/build/plugin/images/juce_binarydata_${REPOSITORY_NAME}BinaryData/JuceLibraryCode"

# -- Validate dependencies --
if [ ! -f "${WONDERLAB_DIR}/cmake/WonderlabBuild.cmake" ]; then
    echo " ERROR: Wonderlab not found at ${WONDERLAB_DIR}"
    echo "         Clone it alongside this repo first."
    exit 1
fi

if [ ! -f "${BINARY_DATA_DIR}/BinaryData.h" ]; then
    echo " ERROR: BinaryData.h not found. Run the native build first."
    exit 1
fi

# -- Find Emscripten SDK --
if [ -n "$EMSDK" ]; then
    EMSDK_DIR="$EMSDK"
elif [ -d "$HOME/emsdk" ]; then
    EMSDK_DIR="$HOME/emsdk"
elif [ -d "/opt/emsdk" ]; then
    EMSDK_DIR="/opt/emsdk"
else
    echo " ERROR: Emscripten SDK not found."
    echo "         Set EMSDK env var or install to ~/emsdk"
    exit 1
fi

source "${EMSDK_DIR}/emsdk_env.sh" 2>/dev/null || true
EMCMAKE="${EMSDK_DIR}/upstream/emscripten/emcmake"
EMMAKE="${EMSDK_DIR}/upstream/emscripten/emmake"

# -- Create build directory --
mkdir -p "${WEB_BUILD_DIR}"
cd "${WEB_BUILD_DIR}"

# -- Generate CMakeLists.txt --
echo "         Generating CMakeLists.txt..."
cat > CMakeLists.txt <<EOF
cmake_minimum_required(VERSION 3.15)
project(${REPOSITORY_NAME}Web LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_NAME "${REPOSITORY_NAME}")
set(PLUGIN_VERSION "${VERSION}")
set(PLUGIN_SRC_DIR "${PLUGIN_SRC_DIR}")
set(WONDERLAB_DIR "${WONDERLAB_DIR}")
set(PLUGIN_PROCESSOR_HEADER "PluginProcessor.h")
set(PLUGIN_PROCESSOR_CLASS audio_plugin::AudioPluginAudioProcessor)
set(PLUGIN_EXTRA_INCLUDE_DIRS "${BINARY_DATA_DIR}")
set(PLUGIN_EXTRA_SOURCE_DIRS "${BINARY_DATA_DIR}")

include(\${WONDERLAB_DIR}/cmake/WonderlabBuild.cmake)
EOF

# -- Configure with Emscripten --
echo "         Configuring Emscripten..."
"${EMCMAKE}" cmake -G Ninja -S . -B . -DCMAKE_BUILD_TYPE=Release

# -- Build WASM --
echo "         Compiling WebAssembly..."
"${EMMAKE}" ninja

WASM_SIZE=$(stat -f%z "${REPOSITORY_NAME}Web.wasm" 2>/dev/null || stat -c%s "${REPOSITORY_NAME}Web.wasm" 2>/dev/null || echo "unknown")
echo "         WASM size: ${WASM_SIZE} bytes"
echo ""

# =============================================================================
# STEP 6: Assemble Web App
# =============================================================================

# Return to project root
cd "${PROJECT_ROOT}"

echo " [6/6] Assembling web app..."
echo ""

# -- Copy Wonderlab web host files --
echo "         Copying web host files..."
cp "${WONDERLAB_DIR}/web/index.html" "${WEB_BUILD_DIR}/index.html"

# -- Copy sample audio tracks --
SAMPLE_DIR="${PROJECT_ROOT}/plugin/sample_audio"
TRACK_LIST=""

if [ -d "${SAMPLE_DIR}" ]; then
    echo "         Copying sample audio tracks..."
    mkdir -p "${WEB_BUILD_DIR}/audio"
    for f in "${SAMPLE_DIR}"/*; do
        [ -f "$f" ] || continue
        cp "$f" "${WEB_BUILD_DIR}/audio/"
        fname=$(basename "$f")
        title="${fname%.*}"
        if [ -n "${TRACK_LIST}" ]; then
            TRACK_LIST="${TRACK_LIST}, { title: '${title}', file: 'audio/${fname}' }"
        else
            TRACK_LIST="{ title: '${title}', file: 'audio/${fname}' }"
        fi
        echo "           + ${fname}"
    done
else
    echo "         No sample_audio folder found, skipping."
fi

# -- Generate plugin-config.js --
echo "         Generating plugin-config.js..."
{
  echo "window.WONDERLAB_CONFIG = {"
  echo "    pluginName: '${REPOSITORY_NAME}',"
  echo "    pluginWidth: 750,"
  echo "    pluginHeight: 515,"
  echo "    wasmJs: '${REPOSITORY_NAME}Web.js',"
  echo "    tracks: [${TRACK_LIST}]"
  echo "};"
} > "${WEB_BUILD_DIR}/plugin-config.js"

echo ""
echo " ========================================================================="
echo "   Build complete!"
echo ""
echo "   Native:  build/plugin/${REPOSITORY_NAME}_artefacts/Release/"
echo "   Web:     build/web/"
echo ""
echo "   Starting local server at http://localhost:8080"
echo " ========================================================================="
echo ""

# -- Start Node.js dev server --
node "${WONDERLAB_DIR}/scripts/serve.js" "${WEB_BUILD_DIR}" 8080
