cmake_minimum_required(VERSION 3.22)

project(ToneForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(/FS)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    NeuralAmpModelerCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(NeuralAmpModelerCore)

file(GLOB_RECURSE NAM_SOURCES
    "${neuralampmodelercore_SOURCE_DIR}/NAM/*.cpp"
    "${neuralampmodelercore_SOURCE_DIR}/NAM/*.c"
)

# Build NAMCore as an OBJECT library so MSVC links every .obj directly and
# never strips static ConfigParserRegistry initializers (WaveNet, LSTM, ConvNet, SlimmableContainer, etc.)
add_library(NAMCore OBJECT ${NAM_SOURCES})

target_include_directories(NAMCore PUBLIC
    "${neuralampmodelercore_SOURCE_DIR}"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann"
)

target_compile_definitions(NAMCore PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    NAM_ENABLE_A2_FAST
    NAM_SAMPLE_FLOAT
)

juce_add_gui_app(ToneForge
    PRODUCT_NAME "ToneForge"
    COMPANY_NAME "xCONFLiCTiONx"
    BUNDLE_ID "cc.xconflictionx.toneforge"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
)

# Overwrite JUCE's downsampled icon.ico with our full multi-resolution icon.ico
if(WIN32)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.ico" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/ToneForge_artefacts/JuceLibraryCode")
    add_custom_target(ToneForge_CopyIcon
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.ico"
            "${CMAKE_CURRENT_BINARY_DIR}/ToneForge_artefacts/JuceLibraryCode/icon.ico"
        COMMENT "Copying full multi-resolution icon.ico to build directory"
    )
    add_dependencies(ToneForge_rc_lib ToneForge_CopyIcon)
endif()

juce_add_binary_data(ToneForgeData
    SOURCES
        Resources/icon.png
)

juce_generate_juce_header(ToneForge)

# Set clean and standard output directories for executable binaries
set_target_properties(ToneForge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# Convenience: deploy the executable and factory Resources directly to the top-level bin/ folder
add_custom_command(TARGET ToneForge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ToneForge>"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:ToneForge>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/Resources/Models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/IRs"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/Resources/IRs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Pedals"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/Resources/Pedals"
    COMMENT "Deploying ToneForge.exe and Resources to project bin/ folder"
)

target_sources(ToneForge PRIVATE
    Source/Main.cpp
    Source/MainPanel.cpp
)

target_compile_definitions(ToneForge PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ToneForge,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ToneForge,JUCE_VERSION>"
)

target_include_directories(ToneForge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asiosdk/common
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asiosdk/host
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/asiosdk/host/pc
    "${neuralampmodelercore_SOURCE_DIR}"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/eigen"
    "${neuralampmodelercore_SOURCE_DIR}/Dependencies/nlohmann"
)

target_link_libraries(ToneForge PRIVATE
    ToneForgeData
    NAMCore
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
