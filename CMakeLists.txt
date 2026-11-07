cmake_minimum_required(VERSION 3.22)
project(QUARRY VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE: use a local checkout if present, otherwise fetch it
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(QUARRY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND QUARRY_FORMATS AU)
endif()

# Two builds from one source: an instrument (plays samples from MIDI, records via its audio input)
# and an audio-effect version for dropping straight onto an audio track.
function(quarry_plugin target product code is_synth formats)
    if(is_synth)
        set(cats Instrument Sampler)
        set(autype kAudioUnitType_MusicDevice)
    else()
        set(cats Fx)
        set(autype kAudioUnitType_MusicEffect)
    endif()
    juce_add_plugin(${target}
        COMPANY_NAME "kalabash"
        COMPANY_WEBSITE ""
        COMPANY_COPYRIGHT "kalabash"
        BUNDLE_ID "com.kalabash.${target}"
        PLUGIN_MANUFACTURER_CODE Klbs
        DESCRIPTION "Four-track granular workstation"
        VERSION ${PROJECT_VERSION}
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${product}"
        IS_SYNTH ${is_synth}
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${cats}
        AU_MAIN_TYPE ${autype}
        FORMATS ${formats})

    target_sources(${target} PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

# The instrument also builds the standalone app; the FX version is plugin-only.
set(QUARRY_FX_FORMATS ${QUARRY_FORMATS})
list(REMOVE_ITEM QUARRY_FX_FORMATS Standalone)
quarry_plugin(Quarry   "QUARRY"    Qry4 TRUE  "${QUARRY_FORMATS}")
quarry_plugin(QuarryFX "QUARRY FX" Qrfx FALSE "${QUARRY_FX_FORMATS}")

option(QUARRY_TESTS "Build the offline test harness" OFF)
if(QUARRY_TESTS)
    juce_add_console_app(QuarryTests PRODUCT_NAME "QuarryTests")
    target_sources(QuarryTests PRIVATE tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(QuarryTests PRIVATE JucePlugin_Name="QUARRY" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(QuarryTests PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
endif()
