# IgnisFlux

**IgnisFlux** 是一款专为小米设备设计的 C++ 后台守护进程。  
它能根据实时的电池状态和用户自定义的容量阈值，动态管理系统的**温控策略**与**充电电流**。

---

## 🔧 安装方法

1. 通过 Magisk Manager 刷入模块  
2. 重启设备  
3. 二进制文件将作为后台服务自动启动

## 📂 目录结构（示例）
```
IgnisFlux/
├── params/                                 # 所有用户自定义配置及目录
   ├── charging_thermal/                    # 充电时部署的温控
   ├── discharging_thermal/                 # 放电时部署的温控
   ├── above_threshold_charge_current       # 电量大于阈值时的电流限制
   ├── at_or_below_threshold_charge_current # 电量小于等于阈值时的电流限制
   ├── capacity_threshold                   # 电量切换点，例如90
   ├── is_control_current                   # 电流控制开关
   ├── is_control_thermal                   # 温控控制开关
├── IgnisFlux                               # C++ 二进制文件
├── module.prop
└── service.sh
```
## ⚙️ 配置指南

模块会监控位于 `/data/adb/modules/IgnisFlux/` 目录下的所有配置文件。

### 1. 热控管理

| 文件                  | 示例值          | 说明                              |
|-----------------------|------------------|------------------------------------|
| is_control_thermal    | 1/0 或 true/false | 是否启用温控策略切换              |
| charging_thermal      | [目录]           | 充电时使用的温控配置文件所在目录   |
| discharging_thermal   | [目录]           | 放电时使用的热控配置文件目录 |

**使用方式**：
- 将你自定义的温控文件分别放入 `params/charging_thermal/` 和 `params/discharging_thermal/` 两个文件夹中
- **状态智能切换**：
  - 🔌 **充电中**：自动将 `params/charging_thermal/` 里的文件部署到系统温控路径
  - 🔋 **非充电**：自动切换为 `params/discharging_thermal/` 里的配置
- **关闭时清理**：当 `params/is_control_thermal` 设置为 0/false 时，模块会清除自定义配置，恢复系统默认温控

### 2. 充电电流限制

| 文件                                | 示例值     | 说明                                      |
|-------------------------------------|------------|--------------------------------------------|
| is_control_current                  | 1/0 或 true/false | 是否启用充电电流限制                    |
| capacity_threshold                  | 80         | 电池容量切换阈值（百分比）                |
| at_or_below_threshold_charge_current| 22000000   | 电池 ≤ 阈值 时 的充电电流限制（单位：μA） |
| above_threshold_charge_current      | 1000000    | 电池 > 阈值 时 的充电电流限制（单位：μA） |

> **重要**：电流值必须使用**微安（μA）**单位，范围为`100000`~`22000000`，步长为100000 
> 示例：3000mA = 3000000μA
> **注意**: 电流限制属于逻辑约束，实际物理电流可能与其不一致。
> - 单位：**微安（μA）**
> - 范围：`100000`~`22000000` (`100mA`~`22000mA`)
> - 步长：必须为 100000 的整数倍 (例如3000000=3000mA).

---

## ⚠️ 要求与免责声明

- **环境要求**：需要 Magisk 20.4 或更高版本
- **免责声明**：  
  修改充电电流与热控策略可能对设备硬件造成影响。  
  使用本模块造成的任何损坏，使用者需自行承担全部风险。  
  开发者不对任何不当配置导致的后果负责。
