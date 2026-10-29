cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(PassiveMasterEQ VERSION 1.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # static runtime: the plugin works without installing the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- JUCE ---------------------------------------------------------------------
# Either point at a local checkout:  cmake -B build -DJUCE_DIR=/path/to/JUCE
# or let CMake download JUCE 8 automatically.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

option(PPM_COPY_AFTER_BUILD "Install the plugin into the system plugin folders after building" OFF)
option(PPM_HEADLESS_LINUX   "CI only: build on Linux without Xrandr/Xinerama/Xcursor/ALSA dev headers" OFF)

juce_add_plugin(PassiveMasterEQ
    PRODUCT_NAME              "MasterDesk Mastering Suite"
    COMPANY_NAME              "Basemental"
    COMPANY_WEBSITE           ""
    BUNDLE_ID                 com.basemental.masterdeskpassiveeq
    PLUGIN_MANUFACTURER_CODE  Bsmt                     # 4 chars, >=1 uppercase: identifies you to hosts
    PLUGIN_CODE               Mdeq                     # 4 chars, unique per plugin; never change after release
    VERSION                   ${PROJECT_VERSION}
    FORMATS                   VST3 AU Standalone       # AU is built on macOS only
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE              kAudioUnitType_Effect
    VST3_CATEGORIES           Fx EQ
    COPY_PLUGIN_AFTER_BUILD   ${PPM_COPY_AFTER_BUILD})

target_sources(PassiveMasterEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(PassiveMasterEQ PRIVATE Source)

target_compile_definitions(PassiveMasterEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(PPM_HEADLESS_LINUX)
    target_compile_definitions(PassiveMasterEQ PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0
        JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(PassiveMasterEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Tests --------------------------------------------------------------------
option(PPM_BUILD_TESTS "Build the offline DSP and processor tests" OFF)
if(PPM_BUILD_TESTS)
    enable_testing()
    add_executable(model_test tests/model_test.cpp)
    target_include_directories(model_test PRIVATE Source)
    add_test(NAME model_test COMMAND model_test)
    add_executable(comp_test tests/comp_test.cpp)
    target_include_directories(comp_test PRIVATE Source)
    add_test(NAME comp_test COMMAND comp_test)
    add_executable(loudness_test tests/loudness_test.cpp)
    target_include_directories(loudness_test PRIVATE Source)
    add_test(NAME loudness_test COMMAND loudness_test)
    add_executable(key_test tests/key_test.cpp)
    target_include_directories(key_test PRIVATE Source)
    add_test(NAME key_test COMMAND key_test)
    add_executable(sontec_test tests/sontec_test.cpp)
    target_include_directories(sontec_test PRIVATE Source)
    add_test(NAME sontec_test COMMAND sontec_test)
    add_executable(classa_test tests/classa_test.cpp)
    target_include_directories(classa_test PRIVATE Source)
    add_test(NAME classa_test COMMAND classa_test)
    add_executable(automaster_test tests/automaster_test.cpp)
    target_include_directories(automaster_test PRIVATE Source)
    add_test(NAME automaster_test COMMAND automaster_test)

    juce_add_console_app(host_test PRODUCT_NAME "host_test")
    target_sources(host_test PRIVATE tests/host_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(host_test PRIVATE Source)
    target_compile_definitions(host_test PRIVATE JucePlugin_Name="MasterDesk Mastering Suite"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(PPM_HEADLESS_LINUX)
        target_compile_definitions(host_test PRIVATE
            JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0)
    endif()
    target_link_libraries(host_test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    add_test(NAME host_test COMMAND host_test)
endif()
