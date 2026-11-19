cmake_minimum_required(VERSION 3.22)

project(NeoCloudBurst VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NeoCloudBurst
    COMPANY_NAME "Andreas Neobyte"
    BUNDLE_ID com.aneobyte.neocloudburst
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE AnNb
    PLUGIN_CODE NCld
    FORMATS VST3 LV2 Standalone
    PRODUCT_NAME "NeoCloudBurst"
    LV2URI "https://aneobyte.audio/plugins/neocloudburst"
)

juce_generate_juce_header(NeoCloudBurst)

target_sources(NeoCloudBurst
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/CloudReverbEngine.cpp
        Source/dsp/DemoToneGenerator.cpp
)

target_compile_definitions(NeoCloudBurst
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NeoCloudBurst
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

set(DIST_DIR "${CMAKE_SOURCE_DIR}/Dist")
add_custom_target(copy_dist ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
    COMMENT "Preparing Dist/ folder"
)

add_dependencies(copy_dist NeoCloudBurst_VST3 NeoCloudBurst_LV2)

add_custom_command(TARGET NeoCloudBurst_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}/VST3"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/VST3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:NeoCloudBurst_VST3>/../.."
        "${DIST_DIR}/VST3/NeoCloudBurst.vst3"
    COMMENT "Copying VST3 into Dist/"
)

add_custom_command(TARGET NeoCloudBurst_LV2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}/LV2"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/LV2"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:NeoCloudBurst_LV2>"
        "${DIST_DIR}/LV2/NeoCloudBurst.lv2"
    COMMENT "Copying LV2 into Dist/"
)

juce_add_console_app(NeoCloudBurstSmoke
    PRODUCT_NAME "NeoCloudBurstSmoke"
)

juce_generate_juce_header(NeoCloudBurstSmoke)

target_sources(NeoCloudBurstSmoke
    PRIVATE
        tools/SmokeRender.cpp
        Source/dsp/CloudReverbEngine.cpp
        Source/dsp/DemoToneGenerator.cpp
)

target_compile_definitions(NeoCloudBurstSmoke
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NeoCloudBurstSmoke
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_events
        juce::juce_core
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
)

target_include_directories(NeoCloudBurstSmoke PRIVATE Source)

juce_add_console_app(NeoCloudBurstShowcase
    PRODUCT_NAME "NeoCloudBurstShowcase"
)

juce_generate_juce_header(NeoCloudBurstShowcase)

target_sources(NeoCloudBurstShowcase
    PRIVATE
        tools/ShowcaseRender.cpp
        Source/dsp/CloudReverbEngine.cpp
        Source/dsp/DemoToneGenerator.cpp
)

target_compile_definitions(NeoCloudBurstShowcase
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NeoCloudBurstShowcase
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_events
        juce::juce_core
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
)

target_include_directories(NeoCloudBurstShowcase PRIVATE Source)
