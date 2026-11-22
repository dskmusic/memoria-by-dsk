cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project(Memoria VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- JUCE: third_party/JUCE (offline) or FetchContent (pinned tag) ----
set(MEMORIA_JUCE_TAG "8.0.17-security")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${MEMORIA_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Plugin ----
juce_add_plugin(Memoria
    COMPANY_NAME "DSK Music"
    COMPANY_WEBSITE "https://dskmusic.com"
    PRODUCT_NAME "Memoria by DSK"
    BUNDLE_ID com.dskmusic.memoria
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/branding/memoria-icon-small.png"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/branding/memoria-icon.png"   # regenerate: MemoriaTool icon branding
    PLUGIN_MANUFACTURER_CODE Dskm
    PLUGIN_CODE Mmra
    FORMATS VST3 AU Standalone   # AU only built on macOS
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

# ---- Shared code (plugin + tool/tests compile the same sources) ----
file(GLOB_RECURSE MEMORIA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(FILTER MEMORIA_SOURCES EXCLUDE REGEX ".*/src/PluginEntry\\.cpp$")

file(GLOB MEMORIA_RESOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
if(MEMORIA_RESOURCES)
    juce_add_binary_data(MemoriaData SOURCES ${MEMORIA_RESOURCES})
endif()

add_library(MemoriaShared INTERFACE)
target_sources(MemoriaShared INTERFACE ${MEMORIA_SOURCES})
target_include_directories(MemoriaShared INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(MemoriaShared INTERFACE
    MEMORIA_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)
target_link_libraries(MemoriaShared INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
if(TARGET MemoriaData)
    target_link_libraries(MemoriaShared INTERFACE MemoriaData)
endif()
if(MSVC)
    target_compile_options(MemoriaShared INTERFACE /bigobj /utf-8)
endif()

target_sources(Memoria PRIVATE src/PluginEntry.cpp)
# Windows: the generated .rc embeds icon.ico, but Ninja does not track it -> rebuild the resources when the icon changes
if(WIN32)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/Memoria_artefacts/JuceLibraryCode/Memoria_resources.rc"
        PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Memoria_artefacts/JuceLibraryCode/icon.ico")
endif()
target_link_libraries(Memoria PRIVATE MemoriaShared)

# ---- Tool: unit tests, offline render, preset validation, factory bank, SFZ, screenshots ----
juce_add_console_app(MemoriaTool PRODUCT_NAME "MemoriaTool")
file(GLOB MEMORIA_TOOL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")
target_sources(MemoriaTool PRIVATE ${MEMORIA_TOOL_SOURCES})
target_include_directories(MemoriaTool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_link_libraries(MemoriaTool PRIVATE MemoriaShared)

enable_testing()
add_test(NAME memoria_unit COMMAND MemoriaTool test)
