#!/usr/bin/env bash
# 轮询等待 ADB 设备上线，最多约 110 秒
ADB="D:/android-toolchain/sdk/platform-tools/adb.exe"
for i in $(seq 1 55); do
    LINE=$("$ADB" devices | grep -E "device$" | head -1)
    if [ -n "$LINE" ]; then
        echo "FOUND after $((i * 2))s: $LINE"
        exit 0
    fi
    sleep 2
done
echo "TIMEOUT: no adb device"
"$ADB" devices -l
exit 1
