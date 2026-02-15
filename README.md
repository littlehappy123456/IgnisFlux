# IgnisFlux

**IgnisFlux** is a C++ background daemon designed **specifically for Xiaomi devices**. It dynamically manages system thermal profiles and charging currents based on real-time battery status and user-defined capacity thresholds.

[中文README](https://github.com/littlehappy123456/IgnisFlux/blob/main/README.zh-CN.md)

---

## 🔧 Installation

1. Flash the module via Magisk Manager.
2. Reboot your device.
3. The binary will start automatically as a background service.

## 📂 Directory Structure (Example)

```
IgnisFlux/
├── charging_thermal/      # Profiles deployed when charging
├── discharging_thermal/   # Profiles deployed when discharging
├── above_threshold_charge_current
├── at_or_below_threshold_charge_current
├── capacity_threshold
├── IgnisFlux              # C++ Binary
├── is_control_current
├── is_control_thermal
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

* **Setup**: Place your custom thermal files into `charging_thermal/` and `discharging_thermal/` folders within the module directory.
* **State-Aware Deployment**:
    * 🔌 **Charging Mode**: Automatically deploys files from `charging_thermal/` to the system thermal directory.
    * 🔋 **Discharging Mode**: Automatically switches to files in `discharging_thermal/`.
* **Cleanup**: When `is_control_thermal` is disabled, the module clears custom configs to restore system defaults.

### 2. Charging Current Limits
| File | Example Value | Description |
| :--- | :--- | :--- |
| `is_control_current` | `1`/`0` or `true`/`false` | Enable/Disable current locking. |
| `capacity_threshold` | `80` | The battery % to switch between limits. |
| `at_or_below_threshold_charge_current` | `22000000` | Limit (in μA) when battery ≤ threshold. |
| `above_threshold_charge_current` | `5000000` | Limit (in μA) when battery > threshold. |

> **Note**: Current values must be in **microamperes** (e.g., `3000000` = 3000mA).

---

## ⚠️ Requirements & Disclaimer

- **Compatibility**: **Xiaomi / Redmi / POCO devices only.**
- **Environment**: Requires Magisk 20.4+ for service execution.
- **Disclaimer**: Modifying charging currents and thermal profiles can affect device hardware. Use this module at your own risk. The developer is not responsible for any damage caused by improper configuration.
