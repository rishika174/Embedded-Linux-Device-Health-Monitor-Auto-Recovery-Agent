# Embedded Linux Device Health Monitor & Auto-Recovery Agent
## System Architecture & Technical Specification

### Monitored Subsystems & Linux Kernel Interfaces

1. CPU: /proc/stat delta jiffies calculation
2. RAM: /proc/meminfo MemAvailable tracking & drop_caches
3. Disk: POSIX statvfs() block usage & temp sweeping
4. Thermal: /sys/class/thermal/thermal_zone0/temp & governor powersave
5. Network: sysfs carrier & ping socket connect
6. Service: systemctl is-active & exponential backoff restart
