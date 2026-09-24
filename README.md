# Embedded Linux Device Health Monitor & Auto-Recovery Agent

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Embedded%20Linux%20%7C%20Yocto%20%7C%20Buildroot%20%7C%20Raspberry%20Pi-green.svg)]()

A lightweight, robust background supervisor daemon written in modern C++17 for embedded Linux devices (IoT gateways, industrial controllers, automotive telematics units, and edge nodes). Continuously monitors 6 critical hardware and kernel subsystems and automatically executes targeted recovery routines upon threshold violations.

---

## Key Features

- **Direct Linux Kernel Interfaces**:
  - **CPU Monitor**: Parses `/proc/stat` delta slices across sampling windows; detects runaway rogue processes in `/proc/[pid]/stat`.
  - **RAM Monitor**: Reads `/proc/meminfo` (`MemAvailable`, `MemTotal`, `SwapFree`) with dirty buffer flushing and cache dropping (`/proc/sys/vm/drop_caches`).
  - **Disk Monitor**: Queries POSIX `statvfs()` on critical mountpoints with automated log rotation and `/tmp` stale artifact reclamation.
  - **Temperature Monitor**: Reads thermal zone millidegrees via `/sys/class/thermal/thermal_zone*/temp` with thermal throttling and cooling intervention.
  - **Network Monitor**: Inspects sysfs carrier states (`/sys/class/net/<iface>/operstate`), bandwidth stats in `/proc/net/dev`, and gateway ICMP/socket health with automated interface re-binding.
  - **Critical Service Monitor**: Supervises crucial systemd units or process PIDs with automatic restart policies, exponential backoff, and crash loop dampening.
- **Autonomous Recovery Agent**: Closed-loop state machine with configurable cooldown periods and retry counters to prevent thrashing.
- **Embedded Footprint**: Zero heavy external runtime dependencies, minimal RAM footprint (< 8MB RSS), low CPU overhead (< 0.5%), signal-safe termination (`SIGTERM`, `SIGINT`, `SIGHUP`).
- **Production Integration**: Complete systemd service unit, install/uninstall scripts, and fault-injection stress testing suite.

---

## Directory Layout

```text
embedded-linux-health-monitor/
├── README.md                          # Project documentation and operational guide
├── CMakeLists.txt                     # Modern CMake build configuration
├── .gitignore                         # Build artifact ignore rules
│
├── config/
│   └── health_monitor.json            # Subsystem thresholds & recovery settings
│
├── include/
│   ├── cpu_monitor.h                  # CPU sampling & thread accounting
│   ├── memory_monitor.h               # RAM & buffer cache analysis
│   ├── disk_monitor.h                 # VFS storage monitoring
│   ├── temperature_monitor.h          # Sysfs thermal zone watcher
│   ├── network_monitor.h              # Link carrier & socket latency
│   ├── service_monitor.h              # Systemd/Process watchdog
│   ├── logger.h                       # Thread-safe journal/file logger
│   └── config_manager.h               # JSON configuration parser
│
├── src/
│   ├── main.cpp                       # Daemon main loop & signal handling
│   ├── cpu_monitor.cpp                # /proc/stat delta parser
│   ├── memory_monitor.cpp             # /proc/meminfo evaluator
│   ├── disk_monitor.cpp               # statvfs() system caller
│   ├── temperature_monitor.cpp        # sysfs thermal reader
│   ├── network_monitor.cpp            # sysfs carrier & socket ping
│   ├── service_monitor.cpp            # Systemctl watchdog runner
│   ├── logger.cpp                     # Log sink implementation
│   └── config_manager.cpp             # Config deserialization logic
│
├── systemd/
│   └── device-health-monitor.service  # Systemd daemon unit file
│
├── scripts/
│   ├── install.sh                     # Automated deployment script
│   ├── uninstall.sh                   # Clean teardown script
│   └── simulate_failure.sh            # Chaos / fault-injection simulator
│
├── tests/
│   ├── test_cpu.cpp                   # CPU monitor test suite
│   ├── test_memory.cpp                # Memory monitor test suite
│   ├── test_config.cpp                # JSON config parser tests
│   └── test_service_monitor.cpp       # Service watcher mock tests
│
└── docs/
    ├── architecture.md                # Subsystem architecture & state machines
    └── test-report.md                 # Verification & benchmarks
```

---

## Quick Start & Compilation

```bash
# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run foreground
sudo ./device_health_monitor --config ../config/health_monitor.json
```
