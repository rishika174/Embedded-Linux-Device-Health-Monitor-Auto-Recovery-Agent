cmake_minimum_required(VERSION 3.14)
project(EmbeddedLinuxHealthMonitor
    VERSION 1.2.0
    DESCRIPTION "Embedded Linux Device Health Monitor & Auto-Recovery Agent"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization & hardening flags for embedded targets
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wshadow
        -O2
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
        -pthread
    )
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(MONITOR_SOURCES
    src/cpu_monitor.cpp
    src/memory_monitor.cpp
    src/disk_monitor.cpp
    src/temperature_monitor.cpp
    src/network_monitor.cpp
    src/service_monitor.cpp
    src/logger.cpp
    src/config_manager.cpp
)

add_library(health_monitor_core STATIC
    ${MONITOR_SOURCES}
)

target_link_libraries(health_monitor_core PUBLIC
    pthread
)

add_executable(device_health_monitor
    src/main.cpp
)

target_link_libraries(device_health_monitor PRIVATE
    health_monitor_core
)

install(TARGETS device_health_monitor DESTINATION /usr/local/bin)
install(FILES config/health_monitor.json DESTINATION /etc/health_monitor)
install(FILES systemd/device-health-monitor.service DESTINATION /etc/systemd/system)

enable_testing()

add_executable(test_cpu tests/test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE health_monitor_core)
add_test(NAME TestCpuMonitor COMMAND test_cpu)

add_executable(test_memory tests/test_memory.cpp)
target_link_libraries(test_memory PRIVATE health_monitor_core)
add_test(NAME TestMemoryMonitor COMMAND test_memory)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE health_monitor_core)
add_test(NAME TestConfigManager COMMAND test_config)

add_executable(test_service_monitor tests/test_service_monitor.cpp)
target_link_libraries(test_service_monitor PRIVATE health_monitor_core)
add_test(NAME TestServiceMonitor COMMAND test_service_monitor)
