#!/usr/bin/env bash
# Failure Simulation & Fault Injection Test Script
# My Labs Embedded Systems

simulate_cpu_spike() {
  echo "Injecting CPU load spike (>90%)..."
  for i in $(seq 1 $(nproc || echo 2)); do
    yes > /dev/null 2>&1 &
  done
  sleep 10
  killall yes 2>/dev/null
}

simulate_memory_leak() {
  echo "Injecting RAM exhaustion..."
  python3 -c "b = bytearray(300 * 1024 * 1024); import time; time.sleep(6)" 2>/dev/null
}

simulate_disk_fill() {
  echo "Injecting Disk mount bloat..."
  dd if=/dev/zero of=/tmp/bloat.dat bs=1M count=200 2>/dev/null
  sleep 4
  rm -f /tmp/bloat.dat
}

simulate_service_crash() {
  echo "Injecting Critical Service SIGKILL..."
  pkill -9 -f "iot-gateway" 2>/dev/null || true
  systemctl stop iot-gateway 2>/dev/null || true
}
