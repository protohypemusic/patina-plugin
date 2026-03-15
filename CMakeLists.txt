cmake_minimum_required(VERSION 3.22)

project(Patina VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(Patina
    COMPANY_NAME "Max Pote"
    PLUGIN_MANUFACTURER_CODE Fpms
    PLUGIN_CODE Ptna
    FORMATS VST3 AU
    PRODUCT_NAME "Patina"
    BUNDLE_ID "com.futureproofmusic.patina"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx"
)

juce_add_binary_data(PatinaAssets
    SOURCES
        assets/Panchang-Bold.otf
)

target_sources(Patina
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/RandomizeSystem.cpp
        src/PresetManager.cpp
        src/PatinaLookAndFeel.cpp
        src/dsp/NoiseModule.cpp
        src/dsp/WobbleModule.cpp
        src/dsp/DistortModule.cpp
        src/dsp/SpaceModule.cpp
        src/dsp/ResonatorModule.cpp
        src/visualizer/SpectralBloomComponent.cpp
)

target_include_directories(Patina
    PRIVATE
        src
)

target_compile_definitions(Patina
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Patina
    PRIVATE
        PatinaAssets
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
