cmake_minimum_required(VERSION 3.25)
project(Shiboleth VERSION 0.5.0 LANGUAGES C CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CLAP_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap" CACHE PATH "")
set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst3sdk" CACHE PATH "")
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "")
set(CLAP_WRAPPER_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(third_party/clap-wrapper EXCLUDE_FROM_ALL)
add_library(shiboleth_impl INTERFACE)
target_include_directories(shiboleth_impl INTERFACE third_party/clap/include)
target_link_libraries(shiboleth_impl INTERFACE gdiplus ole32 gdi32 user32)
# The wrapper uses this variable when checking Windows resources; artwork is
# embedded via the RC file, so no external resource directory is needed.
set(TCLP_RESOURCE_DIRECTORY "")
make_clapfirst_plugins(TARGET_NAME shiboleth IMPL_TARGET shiboleth_impl
    OUTPUT_NAME Shiboleth ENTRY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/plugin.cpp"
    BUNDLE_VERSION 0.5.0 BUNDLE_IDENTIFIER local.abdicant.shiboleth
    COPY_AFTER_BUILD FALSE WINDOWS_FOLDER_VST3 TRUE PLUGIN_FORMATS CLAP VST3
    ASSET_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
target_sources(shiboleth_vst3 PRIVATE src/resources.rc)
target_include_directories(shiboleth_vst3 PRIVATE src)
if(MINGW)
    target_link_options(shiboleth_vst3 PRIVATE -static)
endif()

set_target_properties(shiboleth_vst3 PROPERTIES PREFIX "")
target_sources(shiboleth_clap PRIVATE src/resources.rc)
target_include_directories(shiboleth_clap PRIVATE src)
set_target_properties(shiboleth_clap PROPERTIES PREFIX "")
if(MINGW)
    target_link_options(shiboleth_clap PRIVATE -static)
endif()
add_executable(vst3_tests tests/vst3_tests.cpp
    third_party/vst3sdk/public.sdk/source/vst/hosting/hostclasses.cpp
    third_party/vst3sdk/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
    third_party/vst3sdk/public.sdk/source/vst/hosting/eventlist.cpp
    third_party/vst3sdk/public.sdk/source/vst/hosting/parameterchanges.cpp)
target_link_libraries(vst3_tests PRIVATE base-sdk-vst3 gdiplus ole32 gdi32 user32)
if(MINGW)
    target_link_options(vst3_tests PRIVATE -static)
endif()

target_compile_definitions(shiboleth_vst3 PRIVATE SHIBOLETH_VST3=1)

target_compile_definitions(shiboleth_vst3-clap-wrapper-vst3-lib PRIVATE SHIBOLETH_SAMPLE_RATE_LIMITS=1)
