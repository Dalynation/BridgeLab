cmake_minimum_required(VERSION 3.25)
project(BridgeLab VERSION 0.10.1 LANGUAGES C CXX)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
add_subdirectory("${VST3_SDK_ROOT}" vst3sdk)
smtg_add_vst3plugin(BridgeLabInstrument src/plugin/bridge.cpp src/plugin/auth.mm src/plugin/editor_loader.cpp "${VST3_SDK_ROOT}/public.sdk/source/main/macmain.cpp")
set_source_files_properties(src/plugin/auth.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_compile_features(BridgeLabInstrument PRIVATE cxx_std_17)
target_link_libraries(BridgeLabInstrument PRIVATE sdk)
smtg_target_set_bundle(BridgeLabInstrument BUNDLE_IDENTIFIER "local.bridgelab.instrument" COMPANY_NAME "BridgeLab")
add_executable(bridge-smoke tests/vst3_smoke.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp" "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-smoke PRIVATE cxx_std_17)
target_link_libraries(bridge-smoke PRIVATE sdk_hosting "-framework Cocoa")
add_executable(bridge-auth-gate tests/auth_gate.cpp "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-auth-gate PRIVATE cxx_std_17)
target_link_libraries(bridge-auth-gate PRIVATE sdk_hosting "-framework Cocoa")
target_compile_options(bridge-auth-gate PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")
target_compile_options(bridge-smoke PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")
# The SDK bundle helper sets these fields only with the Xcode generator.
# Supply a complete plugin plist for command-line-tools/Makefile builds too.
set_target_properties(BridgeLabInstrument PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/Info.plist.in")

target_link_libraries(BridgeLabInstrument PRIVATE "-framework Cocoa")
set_source_files_properties(src/plugin/editor.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

add_executable(bridge-multi tests/multi_plugin.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp" "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-multi PRIVATE cxx_std_17)
target_link_libraries(bridge-multi PRIVATE sdk_hosting "-framework Cocoa")

target_compile_options(bridge-multi PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")

add_library(BridgeLabUI SHARED src/plugin/editor.mm)
target_compile_features(BridgeLabUI PRIVATE cxx_std_17)
target_link_libraries(BridgeLabUI PRIVATE sdk "-framework Cocoa")

add_executable(bridge-editor-attachment tests/editor_attachment.mm)
set_source_files_properties(tests/editor_attachment.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_compile_features(bridge-editor-attachment PRIVATE cxx_std_17)
target_link_libraries(bridge-editor-attachment PRIVATE sdk "-framework Cocoa")

add_executable(bridge-editor-repaint tests/editor_repaint.mm)
set_source_files_properties(tests/editor_repaint.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_compile_features(bridge-editor-repaint PRIVATE cxx_std_17)
target_link_libraries(bridge-editor-repaint PRIVATE sdk sdk_hosting "-framework Cocoa")

add_executable(bridge-lifecycle-qa tests/lifecycle_qa.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp" "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-lifecycle-qa PRIVATE cxx_std_17)
target_link_libraries(bridge-lifecycle-qa PRIVATE sdk_hosting "-framework Cocoa")
target_compile_options(bridge-lifecycle-qa PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")

add_executable(bridge-shutdown-transport tests/shutdown_transport.cpp)
target_compile_features(bridge-shutdown-transport PRIVATE cxx_std_17)
target_link_libraries(bridge-shutdown-transport PRIVATE sdk_hosting)

add_executable(bridge-pipeline-transport tests/pipeline_transport.cpp)
target_compile_features(bridge-pipeline-transport PRIVATE cxx_std_17)
target_link_libraries(bridge-pipeline-transport PRIVATE sdk_hosting)

add_executable(bridge-device tests/vst3_device.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp" "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-device PRIVATE cxx_std_17)
target_link_libraries(bridge-device PRIVATE sdk_hosting "-framework Cocoa" "-framework AudioUnit" "-framework CoreAudio")
target_compile_options(bridge-device PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")

add_executable(bridge-shared-transport tests/shared_transport.cpp)
target_compile_features(bridge-shared-transport PRIVATE cxx_std_17)
target_link_libraries(bridge-shared-transport PRIVATE sdk_hosting)

add_library(BridgeLabRealtime SHARED src/macos_realtime.cpp)
target_compile_features(BridgeLabRealtime PRIVATE cxx_std_17)

add_executable(bridge-programs tests/programs.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp" "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_mac.mm")
target_compile_features(bridge-programs PRIVATE cxx_std_17)
target_link_libraries(bridge-programs PRIVATE sdk_hosting "-framework Cocoa")

target_compile_options(bridge-programs PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fobjc-arc>")
