cmake_minimum_required(VERSION 3.22)

# Version is needed by JUCE.
# =====================================================================
project(${wPLUGIN_NAME} VERSION ${wPROJECT_VERSION})

# Force the use of the static runtime (/MT or /MTd) for MSVC
# =====================================================================
# This MUST be set BEFORE juce_add_plugin() to ensure JUCE targets also use
# static runtime. Using CMAKE_MSVC_RUNTIME_LIBRARY is the modern CMake way
# (3.15+) and is more reliable than string replacement.
# Static runtime means the plugin won't need VC++ Redistributable installed
# on the user's machine - everything needed is bundled into the .exe/.dll
if (MSVC)
    # Set static runtime library for all targets created after this point
    # MultiThreaded = /MT (Release), MultiThreadedDebug = /MTd (Debug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    message(STATUS "MSVC: Using static runtime library (/MT) for all targets")
endif()

# Adding a directory with the library/application name as a subfolder of the
# include folder is a good practice. It helps avoid name clashes later on.
# =====================================================================
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/source")

# Adds a plugin target (that's basically what the Projucer does).
# =====================================================================
juce_add_plugin( ${PROJECT_NAME}
    COMPANY_NAME "${wCOMPANY_NAME}"
    BUNDLE_ID "com.${wCOMPANY_ID}.${wPLUGIN_NAME}"  # Valid bundle identifier without spaces
    IS_SYNTH ${wIS_SYNTH}
    NEEDS_MIDI_INPUT ${wNEEDS_MIDI_INPUT}
    NEEDS_MIDI_OUTPUT ${wNEEDS_MIDI_OUTPUT}
    PLUGIN_MANUFACTURER_CODE ${wPLUGIN_MANUFACTURER_CODE}
    PLUGIN_CODE ${wPLUGIN_CODE}
    FORMATS ${wFORMATS}
    PRODUCT_NAME "${wPLUGIN_NAME}"
    VST3_CATEGORIES ${wPLUGIN_CATEGORY}
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Explicitly set runtime library for the plugin target (belt and suspenders approach)
# =====================================================================
# This ensures the plugin target definitely uses static runtime, even if
# something tries to override it later
if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Pre-Processor Directive for Demo vs. Full Version
# =====================================================================
if (BUILD_DEMO)
    add_definitions(-DDEMO_VERSION)
    # Include demo-specific files, exclude features, etc.
    message(STATUS "Configuring Demo Version")
else()
    # Full version configurations
    message(STATUS "Configuring Full Version")
endif()


# Run the Python script to generate the file list
# =====================================================================
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_CURRENT_SOURCE_DIR}/createSourceList.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE PYTHON_RUN_RESULT
)

if(NOT PYTHON_RUN_RESULT EQUAL "0")
    message(FATAL_ERROR "Failed to run Python script to generate file list")
endif()

# Include the generated file list
# =====================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/sourceListResult.cmake)


# Console out the value of PLUGIN_SOURCES
# =====================================================================
message(STATUS "Source files for ${PROJECT_NAME}: ${PLUGIN_SOURCES}")

# Sets the source files of the plugin project.
# =====================================================================
target_sources(${PROJECT_NAME}
    PRIVATE
        ${PLUGIN_SOURCES}
)

# Sets the include directories of the plugin project.
# =====================================================================
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to all necessary dependencies. The present ones are recommended by JUCE.
# If you use one of the additional modules, like the DSP module, you need to specify it here.
# =====================================================================
message(STATUS "Plugin: CMakeLists.txt - target_link_libraries")
target_link_libraries(${PROJECT_NAME}
    PRIVATE
	    ${PROJECT_NAME}BinaryData
        juce::juce_audio_utils
	    juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# These definitions are recommended by JUCE.
# =====================================================================
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
	JUCE_WIN_PER_MONITOR_DPI_AWARE=1
)

# Enables all warnings and treats warnings as errors.
# =====================================================================
# This needs to be set up only for your projects, not 3rd party
#if (MSVC)
#    target_compile_options(${PROJECT_NAME} PRIVATE /Wall /WX)
#    # Silences some deprecation warnings that (hopefully) will be fixed in a future release of JUCE.
#    target_compile_definitions(${PROJECT_NAME}
#        PRIVATE
#            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)
#else()
#    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
#endif()


# Define and create source groups for JUCE files
# =====================================================================
file(GLOB_RECURSE ALL_JUCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../libs/juce/modules/juce_*")
source_group("JUCE Modules" FILES ${ALL_JUCE_FILES})