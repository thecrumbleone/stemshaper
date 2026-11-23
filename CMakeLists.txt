cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) for Logic Pro. Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(StepShaper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout with -DJUCE_DIR=/path/to/JUCE, otherwise download it.
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(StepShaper
    COMPANY_NAME "StepShaper Audio"
    BUNDLE_ID "com.stepshaperaudio.stepshaper"
    PLUGIN_MANUFACTURER_CODE Stpa
    PLUGIN_CODE Ssh1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "StepShaper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Modulation
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(StepShaper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StepEditor.cpp
    Source/Presets.cpp)

target_compile_definitions(StepShaper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(StepShaper
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless test (DSP + presets + GUI snapshot). Off by default.
option(STEPSHAPER_BUILD_TESTS "Build the StepShaper test runner" OFF)
if(STEPSHAPER_BUILD_TESTS)
    juce_add_console_app(StepShaperTests PRODUCT_NAME "StepShaperTests")
    target_sources(StepShaperTests PRIVATE
        Tests/TestMain.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/StepEditor.cpp
        Source/Presets.cpp)
    target_compile_definitions(StepShaperTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 STEPSHAPER_TESTS=1
        JucePlugin_Name="StepShaper" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_link_libraries(StepShaperTests PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
