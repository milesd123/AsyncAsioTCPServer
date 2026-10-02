cmake_minimum_required(VERSION 3.20)
project(AsyncAsioTCPServer VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer (separate build)" OFF)
if(ENABLE_SANITIZERS AND ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer require separate builds")
endif()
if(MSVC AND (ENABLE_SANITIZERS OR ENABLE_THREAD_SANITIZER))
    message(FATAL_ERROR "These sanitizer configurations require GCC or Clang on a supported platform")
endif()
find_package(Threads REQUIRED)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
if(MSVC)
    target_compile_options(project_options INTERFACE /W4 /utf-8)
else()
    target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic)
    if(ENABLE_SANITIZERS)
        target_compile_options(project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(project_options INTERFACE -fsanitize=address,undefined)
    elseif(ENABLE_THREAD_SANITIZER)
        target_compile_options(project_options INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(project_options INTERFACE -fsanitize=thread)
    endif()
endif()

# Bundled, versioned headers allow builds without downloading dependencies.
add_library(network_dependencies INTERFACE)
target_include_directories(network_dependencies SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/asio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/json/include")
target_compile_definitions(network_dependencies INTERFACE ASIO_STANDALONE)
target_link_libraries(network_dependencies INTERFACE Threads::Threads)
if(WIN32)
    target_compile_definitions(network_dependencies INTERFACE _WIN32_WINNT=0x0601)
    target_link_libraries(network_dependencies INTERFACE ws2_32 mswsock)
endif()

add_library(protocol_codec src/code/VarInt.cpp src/code/Protocol.cpp)
target_include_directories(protocol_codec PUBLIC src/headers)
target_link_libraries(protocol_codec PUBLIC project_options)

add_library(proxy_core src/code/Session.cpp src/code/MCPacketReader.cpp
    src/code/Server.cpp src/code/ServerConfig.cpp)
target_link_libraries(proxy_core PUBLIC protocol_codec network_dependencies)

add_executable(ProxyServer src/code/main.cpp)
target_link_libraries(ProxyServer PRIVATE proxy_core)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(protocol_tests tests/protocol_tests.cpp)
    target_link_libraries(protocol_tests PRIVATE proxy_core)
    foreach(suite IN ITEMS varint protocol config)
        add_test(NAME ${suite} COMMAND protocol_tests ${suite})
    endforeach()
    add_test(NAME integration COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_tests.py" "$<TARGET_FILE:ProxyServer>")
    set_tests_properties(integration PROPERTIES TIMEOUT 90)
endif()
