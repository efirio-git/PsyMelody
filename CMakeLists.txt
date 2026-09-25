cmake_minimum_required(VERSION 3.22)
project(PsyMelody VERSION 0.1.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Link the C/C++ runtime statically so the plugin loads on machines
    # without the Visual C++ Redistributable installed
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE)

if(MSVC)
    # Sources are UTF-8; without this MSVC on non-UTF-8 code pages (e.g. CP932) warns with C4819
    add_compile_options(/utf-8)
endif()

juce_add_plugin(PsyMelody
    COMPANY_NAME "EDEN"
    PLUGIN_MANUFACTURER_CODE PsyM
    PLUGIN_CODE PsMl
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "PsyMelody"
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_SYNTH TRUE
)

target_sources(PsyMelody PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/GoaMelodyGenerator.cpp
    Source/GoaMelodyGenerator.h
    Source/ScaleDefinitions.h
    Source/FontUtils.h
    Source/MidiPatternEngine.cpp
    Source/MidiPatternEngine.h
    Source/MidiExport.h
    Source/Presets.h
    Source/Localization.h
    Source/SettingsPage.h
    Source/PsyMelodyLookAndFeel.h
    Source/BasslineGenerator.h
    Source/ChordVoicingGenerator.h
    Source/PreviewSynth.h
    Source/PreviewSynth.cpp
)

juce_add_binary_data(PsyMelodyAssets SOURCES
    Resources/Fonts/Orbitron-Bold.ttf
    Resources/Fonts/Orbitron-Regular.ttf
    Resources/Fonts/Inter-Regular.ttf
    Resources/Fonts/Inter-Medium.ttf
    Resources/Fonts/Inter-Bold.ttf
)

target_compile_definitions(PsyMelody PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

target_link_libraries(PsyMelody
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        PsyMelodyAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
