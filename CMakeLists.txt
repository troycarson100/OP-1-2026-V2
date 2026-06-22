cmake_minimum_required(VERSION 3.22)

project(SculptSampler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives outside the engine: use a local checkout when available,
# otherwise fetch it. The portable engine code never touches it.
set(SCULPT_LOCAL_JUCE "$ENV{HOME}/Documents/JUCE" CACHE PATH "Path to a local JUCE checkout")

if(EXISTS "${SCULPT_LOCAL_JUCE}/CMakeLists.txt")
    message(STATUS "Using local JUCE at ${SCULPT_LOCAL_JUCE}")
    add_subdirectory("${SCULPT_LOCAL_JUCE}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "Local JUCE not found - fetching")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SculptSampler
    COMPANY_NAME "SculptLab"
    BUNDLE_ID com.SculptLab.SculptSampler   # stable id: macOS keys file permissions to it
    PLUGIN_MANUFACTURER_CODE Sclb
    PLUGIN_CODE Scs4
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "SculptSampler"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "SculptSampler captures audio input into track material buffers.")

target_sources(SculptSampler PRIVATE
    # JUCE wrapper layer (the only JUCE-dependent code)
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/juce/InstrumentPanel.cpp
    Source/ui/juce/ModPagePanel.cpp
    Source/ui/juce/SpaceVisual.cpp
    Source/ui/juce/SampleBrowser.cpp

    # Portable engine - core
    Source/core/Engine.cpp
    Source/core/Transport.cpp
    Source/core/ParameterState.cpp
    Source/core/Clock.cpp
    Source/core/Scene.cpp
    Source/core/SceneManager.cpp

    # Portable engine - audio
    Source/audio/Track.cpp
    Source/audio/TrackEngine.cpp
    Source/audio/MaterialSource.cpp
    Source/audio/SampleBuffer.cpp
    Source/audio/SampleRecorder.cpp
    Source/audio/TapePlayer.cpp
    Source/audio/TapeGrainScrub.cpp
    Source/audio/GranularEngine.cpp
    Source/audio/GrainVoice.cpp
    Source/audio/GrainPool.cpp
    Source/audio/FilterStage.cpp
    Source/audio/SpectralFilterStage.cpp
    Source/audio/ColorStage.cpp
    Source/audio/SpaceStage.cpp
    Source/audio/SpaceSendBus.cpp
    Source/audio/SampleBank.cpp
    Source/audio/SpectrumAnalyzer.cpp
    Source/util/RealFFT.cpp
    Source/audio/SpaceDelay.cpp
    Source/audio/HallReverb.cpp
    Source/audio/AllpassDelay.cpp
    Source/audio/MixBusStage.cpp
    Source/audio/Mixer.cpp
    Source/audio/Envelope.cpp
    Source/audio/OnePole.cpp

    # Portable engine - modulation
    Source/modulation/Modulator.cpp
    Source/modulation/LFO.cpp
    Source/modulation/EnvelopeFollower.cpp
    Source/modulation/RandomModulator.cpp
    Source/modulation/ModEngine.cpp
    Source/modulation/AdsrModulator.cpp
    Source/modulation/MacroControls.cpp

    # Hardware abstraction
    Source/hardware/DummyHardware.cpp

    # Abstract screen/page models (no JUCE drawing)
    Source/ui/ScreenModel.cpp
    Source/ui/PageModel.cpp)

target_compile_definitions(SculptSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SculptSampler
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# macOS: re-sign the Standalone app after each build with a stable self-signed identity so the OS
# remembers granted folder-access permissions instead of re-prompting every rebuild (ad-hoc, the
# default, is keyed by the changing binary hash). One-time setup: scripts/setup-macos-codesign.sh.
if(APPLE)
    set(SCULPT_CODESIGN_IDENTITY "SculptSampler Dev"
        CACHE STRING "codesign identity used to stabilise macOS file-access permissions")
    if(TARGET SculptSampler_Standalone)
        add_custom_command(TARGET SculptSampler_Standalone POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DSCULPT_APP=$<TARGET_BUNDLE_DIR:SculptSampler_Standalone>"
                    "-DSCULPT_ID=${SCULPT_CODESIGN_IDENTITY}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CodesignStandalone.cmake"
            VERBATIM)
    endif()
endif()

option(SCULPT_BUILD_TESTS "Build portable engine unit tests" OFF)

if(SCULPT_BUILD_TESTS)
    enable_testing()
    add_executable(sculpt_granular_pattern_tests
        tests/GranularPatternTests.cpp
        Source/audio/GranularEngine.cpp
        Source/audio/GrainPool.cpp
        Source/audio/GrainVoice.cpp
        Source/audio/SampleBuffer.cpp)
    target_include_directories(sculpt_granular_pattern_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/util)
    target_compile_features(sculpt_granular_pattern_tests PRIVATE cxx_std_17)
    add_test(NAME sculpt_granular_pattern COMMAND sculpt_granular_pattern_tests)

    add_executable(sculpt_step_sequencer_tests
        tests/StepSequencerTests.cpp)
    target_include_directories(sculpt_step_sequencer_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/util)
    target_compile_features(sculpt_step_sequencer_tests PRIVATE cxx_std_17)
    add_test(NAME sculpt_step_sequencer COMMAND sculpt_step_sequencer_tests)

    add_executable(sculpt_param_lock_tests
        tests/ParamLockTests.cpp
        Source/core/ParameterState.cpp)
    target_include_directories(sculpt_param_lock_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/util)
    target_compile_features(sculpt_param_lock_tests PRIVATE cxx_std_17)
    add_test(NAME sculpt_param_lock COMMAND sculpt_param_lock_tests)

    add_executable(sculpt_pattern_serialize_tests
        tests/PatternSerializeTests.cpp)
    target_include_directories(sculpt_pattern_serialize_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/util)
    target_compile_features(sculpt_pattern_serialize_tests PRIVATE cxx_std_17)
    add_test(NAME sculpt_pattern_serialize COMMAND sculpt_pattern_serialize_tests)

    add_executable(sculpt_mod_sync_tests
        tests/ModSyncTests.cpp
        Source/modulation/ModEngine.cpp
        Source/modulation/LFO.cpp
        Source/modulation/RandomModulator.cpp
        Source/modulation/AdsrModulator.cpp
        Source/modulation/Modulator.cpp
        Source/core/ParameterState.cpp
        Source/ui/PageModel.cpp)
    target_include_directories(sculpt_mod_sync_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/core
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/util)
    target_compile_features(sculpt_mod_sync_tests PRIVATE cxx_std_17)
    add_test(NAME sculpt_mod_sync COMMAND sculpt_mod_sync_tests)
endif()
