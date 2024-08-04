# ================================================================================================
# runlocalmac.sh
# ================================================================================================

echo "Running with Bash version: $BASH_VERSION"

#!/bin/bash
# Exit on any error
# ================================================================================================
set -e

# Navigate to correct folder
# ================================================================================================
cd "$(dirname "$0")"
cd ..

# Gather project variables from config.cmake
# ================================================================================================
echo "Reading configuration from config.cmake..."

declare -A config_vars
while IFS=' ' read -r key value
do
    value=$(echo $value | tr -d '"')  # Remove quotes
    config_vars[$key]=$value
    echo "$key=${config_vars[$key]}"
done < <(grep 'set(w' ./config.cmake | sed -e 's/set(//g' -e 's/)//g' -e 's/\s\+/ /g')

echo "Variable import complete."

# Set variables from config.cmake
# ================================================================================================
echo "Applying project variables..."

REPOSITORY_NAME=${config_vars[wPLUGIN_NAME]}
FORMATTED_COMPANY_NAME=$(echo "${config_vars[wCOMPANY_NAME]}" | tr -d ' ')
VERSION=${config_vars[wPROJECT_VERSION]}

echo "Applying project variables complete."

# Define the build type
# ================================================================================================
BUILD_TYPE=Release

# Create and navigate to the build directory
# ================================================================================================
echo "Creating build folder."

mkdir -p build/output
cd build

echo "Created build folder."

# Configure the project
# ================================================================================================
echo "Configuring project."

cmake -S .. -B . -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo "Configuration completed successfully."

# Build the project
# ================================================================================================
echo "Building project."

cmake --build . --config $BUILD_TYPE

echo "Build completed successfully."

# Creating packages
# ================================================================================================
echo "Using formatted company name: $FORMATTED_COMPANY_NAME"
echo "Using version: $VERSION"

# Standalone package
# ================================================================================================
echo "Running Standalone Package Build..."

pkgbuild \
    --root "./plugin/${REPOSITORY_NAME}_artefacts/Release/Standalone" \
    --identifier "com.${FORMATTED_COMPANY_NAME}.${REPOSITORY_NAME}.vst3" \
    --version $VERSION \
    --install-location "/Applications/${REPOSITORY_NAME}" \
    "./output/${REPOSITORY_NAME}_Standalone.pkg"

echo "Standalone Package Build complete..."

# VST3 package
# ================================================================================================
echo "Running VST3 Package Build..."

pkgbuild \
    --root "./plugin/${REPOSITORY_NAME}_artefacts/Release/VST3" \
    --identifier "com.${FORMATTED_COMPANY_NAME}.${REPOSITORY_NAME}.vst3" \
    --version $VERSION \
    --install-location "/Library/Audio/Plug-Ins/VST3" \
    "./output/${REPOSITORY_NAME}_VST3.pkg"

echo "VST3 Package Build complete..."

# Copy distribution.xml for productbuild
# ================================================================================================
echo "Copying 'distribution.xml' to build/output folder..."
cp ../build/distribution.xml ../build/output/distribution.xml
echo "File 'distribution.xml' copied to build/output folder."

# Distribution package
# ================================================================================================
echo "Running Product Build..."

productbuild \
    --distribution "./output/distribution.xml" \
    --package-path "./output" \
    "./output/${REPOSITORY_NAME}MacInstaller.pkg"

echo "productbuild complete..."