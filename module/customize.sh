#!/sbin/sh

# Wait for module installation path
MODPATH=${0%/*}

ui_print "- Checking device compatibility..."

# === Step 1: Preserve user's existing params on upgrade ===
if [ -d /data/adb/modules/IgnisFlux/params ]; then
    ui_print "- Upgrading: preserving your existing params..."
    cp -r /data/adb/modules/IgnisFlux/params/* "$MODPATH/params/"
fi

# === Step 2: Device detection ===
IS_CONTROL_THERMAL=true
IS_CONTROL_CURRENT=true
IS_OVERHEAT_PROTECT=true

# Thermal: check Xiaomi-specific nodes
if [ ! -d /mi_ext ] || [ ! -e /dev/mi_display ]; then
    ui_print "- Non-Xiaomi device detected. Disabling thermal control."
    IS_CONTROL_THERMAL=false
fi

# Current control & Overheat: both need constant_charge_current
if [ ! -f /sys/class/power_supply/battery/constant_charge_current ]; then
    ui_print "- Current control node not found. Disabling current control & overheat protection."
    IS_CONTROL_CURRENT=false
    IS_OVERHEAT_PROTECT=false
fi

# Overheat protection: also needs temp sensor
if [ ! -f /sys/class/power_supply/battery/temp ]; then
    ui_print "- Battery temp node not found. Disabling overheat protection."
    IS_OVERHEAT_PROTECT=false
fi

# === Step 3: Abort if nothing works ===
if [ "$IS_CONTROL_THERMAL" = false ] && [ "$IS_CONTROL_CURRENT" = false ] && [ "$IS_OVERHEAT_PROTECT" = false ]; then
    abort "! This module is incompatible with your device!"
fi

# === Step 4: Write overrides based on detection ===
if [ "$IS_CONTROL_THERMAL" = false ]; then
    echo "0" > "$MODPATH/params/is_control_thermal"
fi
if [ "$IS_CONTROL_CURRENT" = false ]; then
    echo "0" > "$MODPATH/params/is_control_current"
fi
if [ "$IS_OVERHEAT_PROTECT" = false ]; then
    echo "0" > "$MODPATH/params/overheat_protect"
fi

set_perm_recursive $MODPATH 0 0 0755 0644
set_perm $MODPATH/IgnisFlux 0 0 0755

ui_print "- Setup completed! *^_^*"
