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

# [Claude 2026-10-27] Copy the built plugin into the system plugin folders (as before). The local test runner
# (tests/run_all.ps1) turns it off: on Windows that copy needs administrator rights.
# =====================================================================
option(WLA_COPY_PLUGIN_AFTER_BUILD "Copy the plugin to the system plugin folders after building" ON)

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
    COPY_PLUGIN_AFTER_BUILD ${WLA_COPY_PLUGIN_AFTER_BUILD}  # [Claude 2026-10-27] was TRUE; still on by default
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/images/icon/tertiary-icon-1024.png"   # [Claude 2026-10-29] the app icon (Joe chose C1)
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/images/icon/tertiary-icon-256.png"
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
# [Claude 2026-10-27] Find Python instead of assuming "python3": on Windows "python3" can be the Microsoft Store
# stub, which fails (it did on the mini PC). Falls back to python3 as before if CMake can't find one.
find_package(Python3 COMPONENTS Interpreter QUIET)
if (NOT Python3_EXECUTABLE)
    set(Python3_EXECUTABLE python3)
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/createSourceList.py
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

# [Claude 2026-10-29] build_id.h (commit, uncommitted changes, time) written before every build, for the About
# window's hidden build number (cmake/build_id.cmake)
add_custom_target(wla_build_id ALL
    COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/generated/build_id.h -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_id.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/generated/build_id.h)
add_dependencies(${PROJECT_NAME} wla_build_id)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated)

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
# ReviewRender (only with -DWLA_REVIEW_RENDER=ON; release and CI builds leave it out): runs Tertiary's processor and
# editor offline for promo and review material, the audio and the UI's frames in sync. See review/ReviewRender.cpp.
option(WLA_REVIEW_RENDER "Build the offline ReviewRender tool" OFF)
if (WLA_REVIEW_RENDER)
    juce_add_console_app(ReviewRender PRODUCT_NAME "ReviewRender")
    target_sources(ReviewRender PRIVATE ${PLUGIN_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/review/ReviewRender.cpp)
    target_compile_definitions(ReviewRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 WLA_REVIEW_RENDER_BUILD=1
        # The plugin macros its sources read, as juce_add_plugin would set them
        JucePlugin_Name="${wPLUGIN_NAME}" JucePlugin_VersionString="${wPROJECT_VERSION}" JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_include_directories(ReviewRender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/generated)
    add_dependencies(ReviewRender wla_build_id)
    target_link_libraries(ReviewRender PRIVATE ${PROJECT_NAME}BinaryData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    if (MSVC)
        set_property(TARGET ReviewRender PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# [Claude 2026-10-27] TertiaryTests (only with -DWLA_TESTS=ON; release builds leave it out): unit tests for the 1.1
# changes (license re-check with a pretend server, sample-rate and tempo changes, no allocation in processBlock,
# state save/restore, demo mode). Run: TertiaryTests  (exit code 0 = all passed). See tests/TertiaryTests.cpp.
option(WLA_TESTS "Build the TertiaryTests unit tests" OFF)
if (WLA_TESTS)
    juce_add_console_app(TertiaryTests PRODUCT_NAME "TertiaryTests")
    target_sources(TertiaryTests PRIVATE ${PLUGIN_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/tests/TertiaryTests.cpp)
    target_compile_definitions(TertiaryTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_UNIT_TESTS=0
        JucePlugin_Name="${wPLUGIN_NAME}" JucePlugin_VersionString="${wPROJECT_VERSION}" JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_include_directories(TertiaryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/source)
    target_link_libraries(TertiaryTests PRIVATE ${PROJECT_NAME}BinaryData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    if (MSVC)
        set_property(TARGET TertiaryTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # [Claude 2026-10-27] HostTest: loads the BUILT plugin (VST3, or the installed AU on a Mac) as a DAW does and runs
    # the manual checklist automatically. See tests/HostTest.cpp. Also built only with -DWLA_TESTS=ON.
    juce_add_console_app(HostTest PRODUCT_NAME "HostTest")
    target_sources(HostTest PRIVATE ${PLUGIN_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/tests/HostTest.cpp)
    target_compile_definitions(HostTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=$<BOOL:${APPLE}>
        JucePlugin_Name="${wPLUGIN_NAME}" JucePlugin_VersionString="${wPROJECT_VERSION}" JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_include_directories(HostTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/source)
    target_link_libraries(HostTest PRIVATE ${PROJECT_NAME}BinaryData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    if (MSVC)
        set_property(TARGET HostTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # [Claude 2026-10-29] UiTest: the real processor and editor in an off-screen window, driven by mouse and keyboard
    # events, checked after every step (parameters, components, the scope, the audio). See tests/UiTest.cpp. Also only
    # with -DWLA_TESTS=ON. WLA_UI_TEST_BUILD points the settings and presets at a temp folder and records links instead
    # of opening them (source/TestHooks.h); no other target defines it.
    # -DWLA_UI_TEST_ASAN=ON builds it with the address sanitiser (for the open/close check, U-22).
    option(WLA_UI_TEST_ASAN "Build UiTest with the address sanitiser" OFF)
    juce_add_console_app(UiTest PRODUCT_NAME "UiTest")
    target_sources(UiTest PRIVATE ${PLUGIN_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/tests/UiTest.cpp)
    target_compile_definitions(UiTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_WIN_PER_MONITOR_DPI_AWARE=1 WLA_UI_TEST_BUILD=1
        UIROBOT_ASAN=$<BOOL:${WLA_UI_TEST_ASAN}>
        JucePlugin_Name="${wPLUGIN_NAME}" JucePlugin_VersionString="${wPROJECT_VERSION}" JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_include_directories(UiTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/source ${CMAKE_CURRENT_BINARY_DIR}/generated)
    add_dependencies(UiTest wla_build_id)
    target_link_libraries(UiTest PRIVATE ${PROJECT_NAME}BinaryData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    if (MSVC)
        set_property(TARGET UiTest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # symbols, so a crash or a hang prints the functions it was in
        target_compile_options(UiTest PRIVATE /Zi)
        target_link_options(UiTest PRIVATE /DEBUG)
    endif()
    if (WLA_UI_TEST_ASAN)
        if (MSVC)
            target_compile_options(UiTest PRIVATE /fsanitize=address /Zi)
            target_link_options(UiTest PRIVATE /INCREMENTAL:NO /DEBUG)
        else()
            target_compile_options(UiTest PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(UiTest PRIVATE -fsanitize=address)
        endif()
    endif()
endif()
