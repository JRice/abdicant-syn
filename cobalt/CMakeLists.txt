cmake_minimum_required(VERSION 3.25)
project(CobaltEditor VERSION 0.1.0 LANGUAGES C CXX RC)
set(CMAKE_CXX_STANDARD 20)
get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CLAP_SDK_ROOT "${REPOSITORY_ROOT}/third_party/clap" CACHE PATH "")
set(VST3_SDK_ROOT "${REPOSITORY_ROOT}/third_party/vst3sdk" CACHE PATH "")
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "")
set(CLAP_WRAPPER_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory("${REPOSITORY_ROOT}/third_party/clap-wrapper" clap-wrapper EXCLUDE_FROM_ALL)
add_library(cobalt_impl INTERFACE)
target_include_directories(cobalt_impl INTERFACE "${CLAP_SDK_ROOT}/include")
target_link_libraries(cobalt_impl INTERFACE gdiplus ole32 gdi32 user32 winmm)
set(TCLP_RESOURCE_DIRECTORY "")
make_clapfirst_plugins(TARGET_NAME cobalt IMPL_TARGET cobalt_impl OUTPUT_NAME CobaltEditor
 ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.cpp" BUNDLE_VERSION 0.1.0
 BUNDLE_IDENTIFIER local.abdicant.cobalt.hardware-editor COPY_AFTER_BUILD FALSE
 WINDOWS_FOLDER_VST3 TRUE PLUGIN_FORMATS CLAP VST3
 ASSET_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
# Reuse the workspace's existing, guarded wrapper sample-rate validation fix.
target_compile_definitions(cobalt_vst3-clap-wrapper-vst3-lib PRIVATE SHIBOLETH_SAMPLE_RATE_LIMITS=1)
foreach(target cobalt_clap cobalt_vst3)
 target_sources(${target} PRIVATE src/version.rc)
 set_target_properties(${target} PROPERTIES PREFIX "")
 if(MINGW)
  target_link_options(${target} PRIVATE -static)
 endif()
endforeach()
add_executable(cobalt_tests tests/plugin_tests.cpp)
target_include_directories(cobalt_tests PRIVATE "${CLAP_SDK_ROOT}/include")
target_link_libraries(cobalt_tests PRIVATE gdiplus ole32 gdi32 user32 winmm)
add_executable(cobalt_transport_tests tests/transport_tests.cpp)
target_link_libraries(cobalt_transport_tests PRIVATE winmm)
add_executable(cobalt_vst3_tests tests/vst3_tests.cpp
 "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/hostclasses.cpp"
 "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp"
 "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/eventlist.cpp"
 "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp")
target_link_libraries(cobalt_vst3_tests PRIVATE base-sdk-vst3 gdiplus ole32 gdi32 user32)
if(MINGW)
 foreach(target cobalt_tests cobalt_transport_tests cobalt_vst3_tests)
  target_link_options(${target} PRIVATE -static)
 endforeach()
endif()
