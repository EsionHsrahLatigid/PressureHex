cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(PressureHex VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(PressureHex VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(PRESSUREHEX_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(PRESSUREHEX_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(PRESSUREHEX_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(PRESSUREHEX_CREATE_AU OFF)
if(APPLE)
    set(PRESSUREHEX_CREATE_AU ON)
endif()
set(PRESSUREHEX_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(pressurehex_engine STATIC
    source/PressureHexEngine.cpp)
target_include_directories(pressurehex_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(pressurehex_engine PUBLIC cxx_std_20)
set_target_properties(pressurehex_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(pressurehex_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PRESSUREHEX_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(pressurehex_engine_tests
        tests/PressureHexEngineTests.cpp)
    target_link_libraries(pressurehex_engine_tests PRIVATE pressurehex_engine)
    target_compile_features(pressurehex_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(pressurehex_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(pressurehex_engine_tests)
    add_test(NAME pressurehex_engine_tests COMMAND pressurehex_engine_tests)
endif()

if(PRESSUREHEX_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${PRESSUREHEX_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${PRESSUREHEX_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME pressurehex
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "PressureHex"
        TARGET_APP_ID "jp.ehl.pressurehex"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.pressurehex"
        PLUGIN_NAME "PressureHex"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Feed-forward log-domain compressor with focusable detector and lookahead"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/PressureHex"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${PRESSUREHEX_CREATE_AU}
        PLUGIN_AU_SUBTYPE "PrHx"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(pressurehex_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/PressureHexPlugin.cpp
        source/PressureHexPlugin.h)
    target_link_libraries(pressurehex_shared INTERFACE pressurehex_engine)
    target_include_directories(pressurehex_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(PRESSUREHEX_BUILD_TESTS)
        add_executable(pressurehex_plugin_bridge_tests
            tests/PressureHexPluginBridgeTests.cpp
            source/PressureHexPlugin.cpp)
        target_link_libraries(pressurehex_plugin_bridge_tests PRIVATE pressurehex_engine yup::yup_audio_processors)
        target_include_directories(pressurehex_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(pressurehex_plugin_bridge_tests PRIVATE PRESSUREHEX_HEADLESS_TEST=1)
        target_compile_features(pressurehex_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(pressurehex_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(pressurehex_plugin_bridge_tests)
        add_test(NAME pressurehex_plugin_bridge_tests COMMAND pressurehex_plugin_bridge_tests)

        add_executable(pressurehex_standalone_bridge_tests
            tests/PressureHexStandaloneBridgeTests.cpp
            source/PressureHexPlugin.cpp)
        target_link_libraries(pressurehex_standalone_bridge_tests PRIVATE pressurehex_engine yup::yup_audio_processors)
        target_include_directories(pressurehex_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(pressurehex_standalone_bridge_tests PRIVATE
            PRESSUREHEX_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(pressurehex_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(pressurehex_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(pressurehex_standalone_bridge_tests)
        add_test(NAME pressurehex_standalone_bridge_tests COMMAND pressurehex_standalone_bridge_tests)
    endif()

    set(pressurehex_release_bundle_targets
        pressurehex_standalone_plugin
        pressurehex_vst3_plugin)
    if(APPLE)
        list(APPEND pressurehex_release_bundle_targets pressurehex_au_plugin)
    endif()

    add_custom_target(pressurehex_release_bundles
        DEPENDS ${pressurehex_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT PressureHex
        SLUG pressurehex
        STANDALONE_TARGET pressurehex_standalone_plugin
        VST3_TARGET pressurehex_vst3_plugin
        AU_TARGET pressurehex_au_plugin)

    if(APPLE)
        set_target_properties(pressurehex_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.pressurehex"
            MACOSX_BUNDLE_BUNDLE_NAME "PressureHex"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.pressurehex")
    endif()

    if(APPLE AND PRESSUREHEX_ADHOC_SIGN)
        find_program(PRESSUREHEX_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET pressurehex_vst3_plugin POST_BUILD
            COMMAND "${PRESSUREHEX_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:pressurehex_vst3_plugin>"
            COMMENT "Ad-hoc signing PressureHex VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET pressurehex_standalone_plugin POST_BUILD
            COMMAND "${PRESSUREHEX_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:pressurehex_standalone_plugin>"
            COMMENT "Ad-hoc signing PressureHex standalone app for local launch"
            VERBATIM)
    endif()
endif()
