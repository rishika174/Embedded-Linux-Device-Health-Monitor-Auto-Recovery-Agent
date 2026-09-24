#!/usr/bin/env bash
# ==============================================================================
# Embedded Linux Device Health Monitor - Automated Installation Script
# My Labs Embedded Systems
# ==============================================================================

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}>>> Starting Device Health Monitor Installation...${NC}"

# Check root privileges
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}[ERROR] This installation script must be executed as root (sudo).${NC}"
  exit 1
fi

BIN_SOURCE="../build/device_health_monitor"
if [ ! -f "$BIN_SOURCE" ]; then
  BIN_SOURCE="build/device_health_monitor"
fi

if [ ! -f "$BIN_SOURCE" ]; then
  echo -e "${YELLOW}[WARN] Compiled binary not found in build/. Attempting local build...${NC}"
  mkdir -p build && cd build
  cmake -DCMAKE_BUILD_TYPE=Release ..
  make -j$(nproc)
  cd ..
  BIN_SOURCE="build/device_health_monitor"
fi

# 1. Install binary
echo -e "${GREEN}[1/5] Installing binary to /usr/local/bin/...${NC}"
install -m 755 "$BIN_SOURCE" /usr/local/bin/device_health_monitor

# 2. Install configuration
echo -e "${GREEN}[2/5] Deploying config to /etc/health_monitor/...${NC}"
mkdir -p /etc/health_monitor
if [ ! -f /etc/health_monitor/health_monitor.json ]; then
  install -m 644 config/health_monitor.json /etc/health_monitor/health_monitor.json
else
  echo -e "${YELLOW}Existing configuration found at /etc/health_monitor/health_monitor.json, preserving.${NC}"
fi

# 3. Create log directory
echo -e "${GREEN}[3/5] Setting up /var/log/device-health-monitor.log...${NC}"
touch /var/log/device-health-monitor.log
chmod 644 /var/log/device-health-monitor.log

# 4. Install systemd service
echo -e "${GREEN}[4/5] Installing systemd service unit...${NC}"
install -m 644 systemd/device-health-monitor.service /etc/systemd/system/device-health-monitor.service
systemctl daemon-reload

# 5. Enable and start daemon
echo -e "${GREEN}[5/5] Enabling and starting device-health-monitor...${NC}"
systemctl enable device-health-monitor
systemctl restart device-health-monitor

echo -e "${GREEN}>>> Installation Complete!${NC}"
echo -e "Check daemon status:   ${YELLOW}systemctl status device-health-monitor${NC}"
echo -e "Stream daemon logs:    ${YELLOW}journalctl -u device-health-monitor -f${NC}"
