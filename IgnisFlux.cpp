#include <fstream>
#include <string>
#include <filesystem>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include <linux/netlink.h>
#include <system_error>
#include <sys/stat.h>

namespace fs = std::filesystem;

// --- 路径配置 ---
const std::string MOD_NAME = "IgnisFlux";
const std::string MOD_DIR = "/data/adb/modules/" + MOD_NAME + "/";
// 所有的参数和子目录现在都在 params/ 下
const std::string PARAMS_DIR = MOD_DIR + "params/";

const std::string THERMAL_SWITCH = PARAMS_DIR + "is_control_thermal";
const std::string CURRENT_SWITCH = PARAMS_DIR + "is_control_current";
const std::string THRESHOLD_FILE = PARAMS_DIR + "capacity_threshold";
const std::string CUR_LOW_FILE = PARAMS_DIR + "at_or_below_threshold_charge_current";
const std::string CUR_HIGH_FILE = PARAMS_DIR + "above_threshold_charge_current";

// 过热保护参数
const std::string OVERHEAT_PROTECT_SWITCH = PARAMS_DIR + "overheat_protect";
const std::string OVERHEAT_TRIGGER_FILE = PARAMS_DIR + "trigger_overheat_threshold";
const std::string OVERHEAT_DISMISS_FILE = PARAMS_DIR + "dismiss_overheat_threshold";
const std::string OVERHEAT_CURRENT_FILE = PARAMS_DIR + "overheat_charge_current";

const std::string CHARGING_THERMAL_DIR = PARAMS_DIR + "charging_thermal/";
const std::string DISCHARGING_THERMAL_DIR = PARAMS_DIR + "discharging_thermal/";
const std::string DST_DIR = "/data/vendor/thermal/config/";

// 内核节点
const std::string BATT_STATUS = "/sys/class/power_supply/battery/status";
const std::string BATT_CAPACITY = "/sys/class/power_supply/battery/capacity";
const std::string BATT_CURR_NODE = "/sys/class/power_supply/battery/constant_charge_current";
const std::string BATT_TEMP = "/sys/class/power_supply/battery/temp";

// 全局状态
bool g_is_control_thermal = false;
bool g_is_control_current = false;
bool g_last_is_powered = false;
bool g_is_charging = false;
int g_capacity_threshold = 80;

// 过热保护全局状态
bool g_overheat_protect = false;       // 过热保护总开关
bool g_overheat_active = false;        // 当前是否处于过热限流状态（带迟滞）
int g_trigger_threshold_c = 43;        // 触发温度阈值，单位℃（默认43℃）
int g_dismiss_threshold_c = 40;        // 解除温度阈值，单位℃（默认40℃）
long g_overheat_charge_current = 1000000;  // 过热时充电电流，单位μA（默认1A）

// --- 工具函数 ---

void add_user_write_permission(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        chmod(path.c_str(), st.st_mode | S_IWUSR);
    }
}

bool is_enabled(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string val;
    file >> val;
    return (val == "1" || val == "true");
}

bool isValidCurrent(long val) {
    return (val >= 100000 && val <= 22000000 && (val % 100000 == 0));
}

/// 过热保护电流允许0值（停止充电），也允许非标准步长
bool isOverheatCurrentValid(long val) {
    return (val >= 0 && val <= 22000000);
}

long readLong(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return -1;
    long val = -1;
    if (!(file >> val)) return -1;
    return val;
}

/// 写入数值到节点
void writeLong(const std::string& path, long val) {
    add_user_write_permission(path);
    std::ofstream outfile(path);
    if (outfile.is_open()) {
        outfile << val;
        outfile.close();
    }
}

/// 读取电池温度原始值（单位：0.1℃）
/// 例如 435 表示 43.5℃
int readBatteryTemp() {
    long raw = readLong(BATT_TEMP);
    if (raw < 0) return -1;
    return (int)raw;
}

// --- 功能模块 ---

void perform_cleanup() {
    std::error_code ec;
    if (!fs::exists(DST_DIR, ec)) return;
    add_user_write_permission(DST_DIR);
    for (const auto& entry : fs::directory_iterator(DST_DIR, ec)) {
        fs::remove_all(entry.path(), ec);
    }
}

void perform_copy(bool cur_powered) {
    std::error_code ec;
    std::string src = cur_powered ? CHARGING_THERMAL_DIR : DISCHARGING_THERMAL_DIR;

    if (!fs::exists(src, ec) || !fs::exists(DST_DIR, ec)) return;

    perform_cleanup();
    add_user_write_permission(DST_DIR);

    for (const auto& entry : fs::directory_iterator(src, ec)) {
        if (entry.is_regular_file()) {
            std::string dest_path = fs::path(DST_DIR) / entry.path().filename();
            fs::copy_file(entry.path(), dest_path, fs::copy_options::overwrite_existing, ec);
            add_user_write_permission(dest_path);
        }
    }
}

// --- 过热保护模块 ---

/// 过热保护逻辑：检测电池温度，管理迟滞状态
/// 仅当设备充电时才生效
void handle_overheat_protection() {
    if (!g_overheat_protect) {
        // 开关关闭 → 重置过热状态（让普通电流控制接管）
        if (g_overheat_active) {
            g_overheat_active = false;
        }
        return;
    }

    // 仅在充电时起作用
    if (!g_last_is_powered) {
        if (g_overheat_active) {
            g_overheat_active = false;  // 拔线后重置状态
        }
        return;
    }

    int temp_raw = readBatteryTemp();
    if (temp_raw < 0) return;

    int trigger_raw = g_trigger_threshold_c * 10;   // ℃ → 0.1℃
    int dismiss_raw = g_dismiss_threshold_c * 10;

    if (g_overheat_active) {
        // 当前处于过热限流状态：检查是否满足解除条件
        if (temp_raw < dismiss_raw) {
            g_overheat_active = false;
            // 如果普通电流控制也关闭，恢复默认22A；否则由 handle_current_control 接管
            if (!g_is_control_current) {
                writeLong(BATT_CURR_NODE, 22000000);
            }
        }
        // 否则持续处于过热状态，电流会由 handle_current_control 中的高优先级逻辑维持
    } else {
        // 当前未过热：检查是否达到触发条件
        if (temp_raw >= trigger_raw) {
            g_overheat_active = true;
            // 立即应用限流
            if (isOverheatCurrentValid(g_overheat_charge_current)) {
                writeLong(BATT_CURR_NODE, g_overheat_charge_current);
            }
        }
    }
}

/// 执行电流控制（过热保护版）
/// 过热保护限流优先级 > 普通电流阶梯控制
void handle_current_control() {
    // === 第一步：过热保护优先 ===
    if (g_overheat_protect && g_overheat_active && g_last_is_powered) {
        if (isOverheatCurrentValid(g_overheat_charge_current)) {
            long current_val = readLong(BATT_CURR_NODE);
            if (current_val != g_overheat_charge_current) {
                writeLong(BATT_CURR_NODE, g_overheat_charge_current);
            }
        }
        return;  // 过热保护已接管，跳过普通电流控制
    }

    // === 第二步：普通电流控制 ===
    if (!g_is_control_current) return;
    // 仅 Charging 状态生效（排除 Full），复用全局状态避免重复读节点
    if (!g_is_charging) return;

    int capacity = (int)readLong(BATT_CAPACITY);
    if (capacity < 0) return;

    std::string target_limit_file = (capacity <= g_capacity_threshold) ? CUR_LOW_FILE : CUR_HIGH_FILE;
    long target_val = readLong(target_limit_file);

    if (isValidCurrent(target_val)) {
        if (readLong(BATT_CURR_NODE) != target_val) {
            writeLong(BATT_CURR_NODE, target_val);
        }
    }
}

void update_all_switches() {
    // 1. 记录旧状态用于对比
    bool old_thermal_switch = g_is_control_thermal;
    bool old_current_switch = g_is_control_current;
    bool old_overheat_protect = g_overheat_protect;

    // 2. 读取新配置
    g_is_control_thermal = is_enabled(THERMAL_SWITCH);
    g_is_control_current = is_enabled(CURRENT_SWITCH);
    g_overheat_protect = is_enabled(OVERHEAT_PROTECT_SWITCH);

    // 常规电流阈值
    long t = readLong(THRESHOLD_FILE);
    g_capacity_threshold = (t > 0 && t <= 100) ? (int)t : 80;

    // 过热保护阈值（文件值：℃）
    long ot = readLong(OVERHEAT_TRIGGER_FILE);
    g_trigger_threshold_c = (ot > 0) ? (int)ot : 43;

    long dt = readLong(OVERHEAT_DISMISS_FILE);
    g_dismiss_threshold_c = (dt > 0) ? (int)dt : 40;

    // 过热充电电流
    long oc = readLong(OVERHEAT_CURRENT_FILE);
    if (isOverheatCurrentValid(oc)) {
        g_overheat_charge_current = oc;
    } else {
        g_overheat_charge_current = 1000000;  // 默认1A
    }

    // 防御性校验：dismiss 必须小于 trigger，否则自锁/振荡
    if (g_dismiss_threshold_c >= g_trigger_threshold_c) {
        g_dismiss_threshold_c = g_trigger_threshold_c - 3;  // 强制留3℃迟滞
        if (g_dismiss_threshold_c < 0) g_dismiss_threshold_c = 0;
    }

    // 3. 获取当前实时电源状态，同步全局
    {
        std::ifstream f(BATT_STATUS);
        std::string s;
        f >> s;
        g_last_is_powered = (s == "Charging" || s == "Full");
        g_is_charging = (s == "Charging");
    }

    // --- 过热保护开关变化处理 ---
    if (old_overheat_protect && !g_overheat_protect) {
        // 过热保护从 ON → OFF：重置状态
        g_overheat_active = false;
        // 如果普通电流控制也关闭，恢复默认22A；否则由 handle_current_control 接管
        if (!g_is_control_current) {
            writeLong(BATT_CURR_NODE, 22000000);
        }
    } else if (g_overheat_protect) {
        // 过热保护开启或持续开启：执行温度检测
        handle_overheat_protection();
    }

    // --- 温控处理逻辑 ---
    if (old_thermal_switch && !g_is_control_thermal) {
        // 开关从 ON 变 OFF：清理部署的温控文件
        perform_cleanup();
    } else if (g_is_control_thermal) {
        // 开关开启：根据当前充电/放电状态同步目录
        perform_copy(g_last_is_powered);
    }

    // --- 电流处理逻辑 (包含恢复逻辑) ---
    if (old_current_switch && !g_is_control_current) {
        // 开关从 ON 变 OFF：有源接管则维持，否则恢复默认22A
        if (g_overheat_protect && g_overheat_active && g_last_is_powered) {
            handle_current_control();  // 过热活跃中，维持过热电流
        } else {
            writeLong(BATT_CURR_NODE, 22000000);  // 不过热，恢复默认
        }
    } else if (g_is_control_current) {
        // 开关开启：执行电流控制（内部按优先级：过热 > 普通）
        handle_current_control();
    }
}

int init_netlink() {
    struct sockaddr_nl sa;
    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = 0xffffffff;
    int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (sock >= 0 && bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

int init_timer() {
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd < 0) return -1;

    struct itimerspec ts;
    ts.it_interval.tv_sec = 5;    // 每5秒触发一次
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = 5;       // 5秒后首次触发
    ts.it_value.tv_nsec = 0;

    if (timerfd_settime(timer_fd, 0, &ts, NULL) < 0) {
        close(timer_fd);
        return -1;
    }
    return timer_fd;
}

int main() {
    // 初始化：先读取电源状态，再执行各项开关逻辑
    {
        std::ifstream f(BATT_STATUS);
        std::string s; f >> s;
        g_last_is_powered = (s == "Charging" || s == "Full");
        g_is_charging = (s == "Charging");
    }
    update_all_switches();

    // 监听 params 目录的变动
    int fd_inotify = inotify_init1(IN_NONBLOCK);
    if (fd_inotify >= 0) {
        inotify_add_watch(fd_inotify, PARAMS_DIR.c_str(), IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB | IN_CREATE | IN_MOVED_TO);
    }

    int fd_netlink = init_netlink();
    int fd_timer = init_timer();  // 定时器用于周期性温度检测

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev, events[3];

    ev.events = EPOLLIN; ev.data.fd = fd_inotify;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_inotify, &ev);
    ev.events = EPOLLIN; ev.data.fd = fd_netlink;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_netlink, &ev);
    if (fd_timer >= 0) {
        ev.events = EPOLLIN; ev.data.fd = fd_timer;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_timer, &ev);
    }

    char buffer[4096];
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 3, -1);
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == fd_inotify) {
                read(fd_inotify, buffer, sizeof(buffer));
                update_all_switches();
            }
            else if (events[n].data.fd == fd_netlink) {
                ssize_t len = recv(fd_netlink, buffer, sizeof(buffer) - 1, 0);
                if (len <= 0) continue;
                buffer[len] = '\0';
                std::string msg(buffer, len);

                if (msg.find("SUBSYSTEM=power_supply") != std::string::npos) {
                    bool g_last_is_powered_old = g_last_is_powered;

                    // 更新电源状态（区分 Charging 和 Full）
                    if (msg.find("POWER_SUPPLY_STATUS=Charging") != std::string::npos) {
                        g_last_is_powered = true;
                        g_is_charging = true;
                    } else if (msg.find("POWER_SUPPLY_STATUS=Full") != std::string::npos) {
                        g_last_is_powered = true;
                        g_is_charging = false;
                    } else if (msg.find("POWER_SUPPLY_STATUS=Discharging") != std::string::npos) {
                        g_last_is_powered = false;
                        g_is_charging = false;
                    }

                    // 电源状态变化时触发过热保护检测
                    if (g_last_is_powered != g_last_is_powered_old && g_overheat_protect) {
                        handle_overheat_protection();
                    }

                    // 温控目录拷贝
                    if (g_is_control_thermal) {
                        if (g_last_is_powered != g_last_is_powered_old) {
                            perform_copy(g_last_is_powered);
                        }
                    }

                    // 电流控制：响应状态/容量/电流/温度变化
                    if (msg.find("POWER_SUPPLY_STATUS=") != std::string::npos ||
                        msg.find("POWER_SUPPLY_CAPACITY=") != std::string::npos ||
                        msg.find("POWER_SUPPLY_CONSTANT_CHARGE_CURRENT=") != std::string::npos ||
                        msg.find("POWER_SUPPLY_TEMP=") != std::string::npos) {
                        handle_current_control();
                    }
                }
            }
            else if (events[n].data.fd == fd_timer) {
                // 定时器到期：定期检测过热保护（uEvent可能不够频繁）
                uint64_t exp;
                read(fd_timer, &exp, sizeof(exp));
                if (g_overheat_protect) {
                    handle_overheat_protection();
                }
                // 如果过热保护未开启但处于过热状态（比如刚关闭开关），也检测
                if (!g_overheat_protect && g_overheat_active) {
                    handle_overheat_protection();
                }
                // 执行电流控制，确保过热电流参数热更新（如修改 overheat_charge_current）立即生效
                handle_current_control();
            }
        }
    }
    return 0;
}
