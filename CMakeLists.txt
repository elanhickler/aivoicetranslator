cmake_minimum_required(VERSION 3.22)
project(AIVoiceTranslator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

include(FetchContent)

set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout")
if(NOT JUCE_DIR)
    set(_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../soemdsp-sandbox-audioplugin/libs/JUCE")
    if(EXISTS "${_sibling}/CMakeLists.txt")
        set(JUCE_DIR "${_sibling}")
    endif()
endif()

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)

FetchContent_Declare(whispercpp
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG v1.7.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(whispercpp)

juce_add_gui_app(AIVoiceTranslator
    COMPANY_NAME "Soundemote"
    PRODUCT_NAME "AI Voice Translator")

juce_generate_juce_header(AIVoiceTranslator)

target_sources(AIVoiceTranslator
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.cpp
        Source/VoiceEngine.cpp)

target_include_directories(AIVoiceTranslator
    PRIVATE
        Source
        ${whispercpp_SOURCE_DIR})

target_compile_definitions(AIVoiceTranslator
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:AIVoiceTranslator,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:AIVoiceTranslator,JUCE_VERSION>")

target_link_libraries(AIVoiceTranslator
    PRIVATE
        whisper
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
