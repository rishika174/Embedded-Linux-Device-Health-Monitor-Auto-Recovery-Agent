#!/usr/bin/env bash
# ==============================================================================
# Embedded Linux Device Health Monitor - Teardown / Uninstallation Script
# ==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}[ERROR] This uninstallation script must be run as root.${NC}"
  exit 1
fi

echo -e "${RED}>>> Stopping and disabling device-health-monitor.service...${NC}"
systemctl stop device-health-monitor 2>/dev/null || true
systemctl disable device-health-monitor 2>/dev/null || true

echo -e ">>> Removing systemd unit..."
rm -f /etc/systemd/system/device-health-monitor.service
systemctl daemon-reload

echo -e ">>> Removing binary..."
rm -f /usr/local/bin/device_health_monitor

echo -e ">>> Removing PID locks..."
rm -f /var/run/device-health-monitor.pid

read -p "Do you also wish to delete configuration files in /etc/health_monitor? [y/N]: " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
  rm -rf /etc/health_monitor
  echo -e "Configuration directory removed."
fi

echo -e "${GREEN}>>> Teardown complete.${NC}"
