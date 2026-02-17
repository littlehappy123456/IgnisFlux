# IgnisFlux

**IgnisFlux** is a C++ background daemon. It dynamically manages system thermal profiles and charging currents based on real-time battery status and user-defined capacity thresholds.

[中文README](https://github.com/littlehappy123456/IgnisFlux/blob/main/README.zh-CN.md)

---

## 🔧 Installation

1. Flash the module via Magisk Manager.
2. Reboot your device.
3. The binary will start automatically as a background service.

## 📂 Directory Structure (Example)

```
IgnisFlux/
├── params/                                 # All user-defined configurations and directories
   ├── charging_thermal/                    # Thermal profiles deployed when charging
   ├── discharging_thermal/                 # Thermal profiles deployed when discharging
   ├── above_threshold_charge_current       # Current limit when battery > threshold
   ├── at_or_below_threshold_charge_current # Current limit when battery ≤ threshold
   ├── capacity_threshold                   # Current switching point, e.g., 90
   ├── is_control_current                   # Current control switch
   ├── is_control_thermal                   # Thermal control switch
├── IgnisFlux                               # C++ Binary
├── module.prop
└── service.sh
```

## ⚙️ Configuration Guide

The module monitors all configuration files located in `/data/adb/modules/IgnisFlux/`.

### 1. Thermal Control
| File | Example Value | Description |
| :--- | :--- | :--- |
| `is_control_thermal` | `1`/`0` or `true`/`false` | Enable/Disable thermal switching. |
| 🔌 `charging_thermal` | `[Directory]` | Thermal profiles deployed when the device is charging. |
| 🔋 `discharging_thermal` | `[Directory]` | Thermal profiles deployed when the device is discharging. |

* **Setup**: Place your custom thermal files into `params/charging_thermal/` and `params/discharging_thermal/` folders within the module directory.
* **State-Aware Deployment**:
    * 🔌 **Charging Mode**: Automatically deploys files from `params/charging_thermal/` to the system thermal directory.
    * 🔋 **Discharging Mode**: Automatically switches to files in `params/discharging_thermal/`.
* **Cleanup**: When `params/is_control_thermal` is disabled, the module clears custom configs to restore system defaults.

### 2. Charging Current Limits
| File | Example Value | Description |
| :--- | :--- | :--- |
| `is_control_current` | `1`/`0` or `true`/`false` | Enable/Disable current locking. |
| `capacity_threshold` | `80` | The battery % to switch between limits. |
| `at_or_below_threshold_charge_current` | `22000000` | Limit (in μA) when battery ≤ threshold. |
| `above_threshold_charge_current` | `1000000` | Limit (in μA) when battery > threshold. |

> **Note**: The current limit is a logical constraint. The actual physical current may not match
> - Unit: **Microamperes (μA)**.
> - Range: `100000` to `22000000` (`100mA` to `22000mA`).
> - Step: Must be a multiple of `100000` (e.g., `3000000` = `3000mA`).
---

## ⚠️ Requirements & Disclaimer

- **Environment**: Requires Magisk 20.4+
- **Disclaimer**: Modifying charging currents and thermal profiles can affect device hardware. Use this module at your own risk. The developer is not responsible for any damage caused by improper configuration.
