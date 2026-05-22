#!/system/bin/sh
MODDIR=${0%/*}

wait_until_boot_completed() {
    until [ "$(getprop sys.boot_completed)" = "1" ]; do
        sleep 1s
    done
}

nohup ${MODDIR}/IgnisFlux >/dev/null 2>&1 &