cmake_minimum_required(VERSION 3.22)
project(FourierTruncator VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FourierTruncator
    COMPANY_NAME "Dan Vicente"
    BUNDLE_ID com.danvicente.fouriertruncator
    PLUGIN_MANUFACTURER_CODE Dvic
    PLUGIN_CODE FTrn
    FORMATS VST3 AU
    PRODUCT_NAME "Fourier Truncator"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(FourierTruncator PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TruncatorEngine.cpp)

target_compile_definitions(FourierTruncator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(FourierTruncator
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline DSP tests: ctest --test-dir build
juce_add_console_app(EngineTests PRODUCT_NAME "EngineTests")
target_sources(EngineTests PRIVATE tests/EngineTests.cpp Source/TruncatorEngine.cpp)
target_compile_definitions(EngineTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(EngineTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)

enable_testing()
add_test(NAME EngineTests COMMAND EngineTests)

# Dev tool: offscreen render of the editor, see tests/UiSnapshot.cpp
juce_add_console_app(UiSnapshot PRODUCT_NAME "UiSnapshot")
target_sources(UiSnapshot PRIVATE tests/UiSnapshot.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/TruncatorEngine.cpp)
target_compile_definitions(UiSnapshot PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Fourier Truncator")
target_link_libraries(UiSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
