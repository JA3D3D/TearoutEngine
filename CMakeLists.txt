cmake_minimum_required(VERSION 3.22)

project(TEAROUT_ENGINE VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(TE_BUILD_PLUGIN "Build the JUCE plugin (VST3 / AU / Standalone)" ON)
option(TE_BUILD_TESTS  "Build the engine tests and the command-line tool" ON)
set(TE_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE 8.0.15)")

# ---- core engine: plain C++17, no JUCE -----------------------------------------------------------
add_library(te_core STATIC
    src/core/Theory.cpp
    src/core/Catalogue.cpp
    src/core/Engine.cpp
    src/core/Seed.cpp
    src/core/MidiFile.cpp
    src/core/Synth.cpp
    src/core/Sample.cpp)
target_include_directories(te_core PUBLIC src/core)
if(MSVC)
    target_compile_options(te_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(te_core PRIVATE -Wall -Wextra)
endif()

if(TE_BUILD_TESTS)
    add_executable(te_tests tests/test_engine.cpp)
    target_link_libraries(te_tests PRIVATE te_core)
    add_executable(te_cli tools/te_cli.cpp)
    target_link_libraries(te_cli PRIVATE te_core)
    enable_testing()
    add_test(NAME engine COMMAND te_tests)
endif()

# ---- plugin ----------------------------------------------------------------------------------------
if(TE_BUILD_PLUGIN)
    if(TE_JUCE_DIR)
        add_subdirectory(${TE_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.15
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(TE_FORMATS VST3 AU Standalone)
    else()
        set(TE_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(TearoutEngine
        PRODUCT_NAME "TEAROUT ENGINE"
        COMPANY_NAME "TEAROUT"
        BUNDLE_ID "com.tearout.engine"
        PLUGIN_MANUFACTURER_CODE Trot
        PLUGIN_CODE Te10
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Instrument Generator
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        MICROPHONE_PERMISSION_ENABLED FALSE
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_512.png
        FORMATS ${TE_FORMATS})

    target_sources(TearoutEngine PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/NeonLookAndFeel.cpp
        src/plugin/Widgets.cpp)

    target_compile_definitions(TearoutEngine PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_XRANDR=0
        JUCE_USE_XINERAMA=0
        JUCE_USE_XCURSOR=0
        JUCE_ALSA=0
        JUCE_JACK=0)

    target_link_libraries(TearoutEngine
        PRIVATE
            te_core
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # headless GUI snapshot tool (renders the editor to a PNG; used for docs and CI)
    if(TE_BUILD_TESTS AND NOT APPLE AND NOT WIN32)
        juce_add_console_app(te_snapshot PRODUCT_NAME "te_snapshot")
        target_sources(te_snapshot PRIVATE
            tools/te_snapshot.cpp
            src/plugin/PluginProcessor.cpp
            src/plugin/PluginEditor.cpp
            src/plugin/NeonLookAndFeel.cpp
            src/plugin/Widgets.cpp)
        target_compile_definitions(te_snapshot PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
            JUCE_ALSA=0 JUCE_JACK=0 JucePlugin_Name="TEAROUT ENGINE" JucePlugin_Build_Standalone=0
            TE_SNAPSHOT=1)
        target_link_libraries(te_snapshot PRIVATE te_core
            juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra
            juce::juce_recommended_config_flags)

        juce_add_console_app(te_plugin_tests PRODUCT_NAME "te_plugin_tests")
        target_sources(te_plugin_tests PRIVATE
            tools/te_plugin_tests.cpp
            src/plugin/PluginProcessor.cpp
            src/plugin/PluginEditor.cpp
            src/plugin/NeonLookAndFeel.cpp
            src/plugin/Widgets.cpp)
        target_compile_definitions(te_plugin_tests PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
            JUCE_ALSA=0 JUCE_JACK=0 JucePlugin_Name="TEAROUT ENGINE" JucePlugin_Build_Standalone=0
            TE_SNAPSHOT=1)
        target_link_libraries(te_plugin_tests PRIVATE te_core
            juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra
            juce::juce_recommended_config_flags)
        add_test(NAME plugin COMMAND te_plugin_tests)

        # loads the built VST3 like a DAW would
        juce_add_console_app(te_host_check PRODUCT_NAME "te_host_check")
        target_sources(te_host_check PRIVATE tools/te_host_check.cpp)
        target_compile_definitions(te_host_check PRIVATE
            JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
            JUCE_ALSA=0 JUCE_JACK=0)
        target_link_libraries(te_host_check PRIVATE
            juce::juce_audio_processors juce::juce_gui_extra juce::juce_recommended_config_flags)
    endif()
endif()
