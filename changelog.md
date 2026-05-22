# V1.1.0
Add overheat protection feature
- New `overheat_protect`, `trigger_overheat_threshold`, `dismiss_overheat_threshold`, `overheat_charge_current` params
- Overheat protection has higher priority than normal current control
- Hysteresis design prevents oscillation around threshold temperatures
- Automatic current recovery when overheat dismisses or protection is disabled
- Temperature is monitored via kernel uevents and periodic polling (every 5 seconds)
- Independent control — works with or without normal current control enabled
- Overheat protection defaults to ON
- customize.sh: preserve user params on module upgrade
- customize.sh: device detection for overheat protection (checks temp + current nodes)
- netlink handler: check overheat state on status/capacity/current uevents (faster response)

# V1.0.2
Fix bugs

# V1.0.1
Fix bugs
Refactor the module directory structure
Relax device restrictions

# V1.0.0
Release
