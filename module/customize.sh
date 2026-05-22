ui_print "- Checking device compatibility..."

IS_CONTROL_THERMAL=true
IS_CONTROL_CURRENT=true

if [ ! -d /mi_ext ] || [ ! -e /dev/mi_display ]; then
    ui_print "- Non-Xiaomi device detected. Disabling thermal control."
    IS_CONTROL_THERMAL=false
fi

if [ ! -f /sys/class/power_supply/battery/constant_charge_current ]; then
    ui_print "- Current control node not found. Disabling current control."
    IS_CONTROL_CURRENT=false
fi

if [ "$IS_CONTROL_THERMAL" = false ] && [ "$IS_CONTROL_CURRENT" = false ]; then
    abort "! This module is incompatible with your device!"
fi

if [ "$IS_CONTROL_THERMAL" = false ]; then
    echo "0" > "$MODPATH/params/is_control_thermal"
fi

if [ "$IS_CONTROL_CURRENT" = false ]; then
    echo "0" > "$MODPATH/params/is_control_current"
fi

chmod +x "$MODPATH/IgnisFlux"

ui_print "- Setup completed! *^_^*"