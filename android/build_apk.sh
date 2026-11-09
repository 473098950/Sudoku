#!/usr/bin/env bash
#
# 手工构建 Android APK（不依赖 Gradle / AndroidX / 任何 Maven 依赖）
#
#   1. aapt2 compile   资源 -> 扁平包
#   2. aapt2 link      资源 + 清单 -> base.apk（含资源表与 R.java）
#   3. javac           源码 + R.java -> .class
#   4. d8              .class -> classes.dex
#   5. jar uf          把 dex 塞进 apk
#   6. zipalign        4 字节对齐
#   7. apksigner       v1+v2+v3 签名
#
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
TOOL="${TOOLCHAIN:-/d/android-toolchain}"

# aapt2 / zipalign / java 都是原生 Windows 程序，不认 MSYS 的 /c/... 路径，
# 统一转成 C:/... 形式（cygpath -m）
WROOT="$(cygpath -m "$ROOT")"
WTOOL="$(cygpath -m "$TOOL")"

SDK="$WTOOL/sdk"
JDK="$WTOOL/jdk17"
BT="$SDK/build-tools/35.0.0"
AJAR="$SDK/platforms/android-35/android.jar"

JAVA="$JDK/bin/java.exe"
JAVAC="$JDK/bin/javac.exe"
JAR="$JDK/bin/jar.exe"
KEYTOOL="$JDK/bin/keytool.exe"
AAPT2="$BT/aapt2.exe"
ZIPALIGN="$BT/zipalign.exe"

OUT="$WROOT/build"
DIST="$WROOT/../dist"

MIN_SDK=30
TARGET_SDK=35
VER_CODE=1
VER_NAME="1.0"

# ---------------------------------------------------------------- 环境自检

for f in "$JAVA" "$AAPT2" "$AJAR"; do
    [ -e "$f" ] || { echo "缺少: $f"; exit 1; }
done

echo "==> 清理"
rm -rf "$OUT"
mkdir -p "$OUT/compiled" "$OUT/gen" "$OUT/classes" "$OUT/dex" "$DIST"

# ---------------------------------------------------------------- 1. 编译资源

echo "==> aapt2 compile"
"$AAPT2" compile --dir "$WROOT/res" -o "$OUT/compiled/res.zip"

# ---------------------------------------------------------------- 2. 链接

echo "==> aapt2 link"
"$AAPT2" link \
    -o "$OUT/base.apk" \
    -I "$AJAR" \
    --manifest "$WROOT/AndroidManifest.xml" \
    -R "$OUT/compiled/res.zip" \
    --java "$OUT/gen" \
    --min-sdk-version $MIN_SDK \
    --target-sdk-version $TARGET_SDK \
    --version-code $VER_CODE \
    --version-name "$VER_NAME" \
    --auto-add-overlay

# ---------------------------------------------------------------- 3. javac

echo "==> javac"
find "$WROOT/src" "$OUT/gen" -name '*.java' > "$OUT/sources.txt"
echo "    源文件数: $(wc -l < "$OUT/sources.txt")"

# 只把 android.jar 放 classpath（不用 -bootclasspath，JDK 9+ 已忽略）
"$JAVAC" \
    -source 11 -target 11 \
    -encoding UTF-8 \
    -nowarn \
    -classpath "$AJAR" \
    -d "$OUT/classes" \
    @"$OUT/sources.txt" 2>&1 | grep -v "^注:" | grep -v "^警告:" || true

# javac 失败时上面被 || true 吞掉了，这里显式检查产物
[ -f "$OUT/classes/com/buddy/sudoku/MainActivity.class" ] || { echo "编译失败"; exit 1; }

# ---------------------------------------------------------------- 4. d8

echo "==> d8"
( cd "$OUT/classes" && "$JAR" cf "$OUT/classes.jar" . )
"$JAVA" -cp "$BT/lib/d8.jar" com.android.tools.r8.D8 \
    --lib "$AJAR" \
    --min-api $MIN_SDK \
    --release \
    --output "$OUT/dex" \
    "$OUT/classes.jar"

# ---------------------------------------------------------------- 5. 打包 dex

echo "==> 写入 classes.dex"
"$JAR" uf "$OUT/base.apk" -C "$OUT/dex" classes.dex

# ---------------------------------------------------------------- 6. 对齐

echo "==> zipalign"
"$ZIPALIGN" -f -p 4 "$OUT/base.apk" "$OUT/aligned.apk"

# ---------------------------------------------------------------- 7. 签名

KS="$WROOT/keystore.jks"
if [ ! -f "$KS" ]; then
    echo "==> 生成签名密钥（首次）"
    "$KEYTOOL" -genkeypair \
        -keystore "$KS" \
        -alias sudoku \
        -keyalg RSA -keysize 2048 -validity 10950 \
        -storepass sudoku123 -keypass sudoku123 \
        -dname "CN=Sudoku, OU=Buddy, O=Buddy, L=Hangzhou, ST=Zhejiang, C=CN" \
        -noprompt
fi

echo "==> apksigner"
"$JAVA" -jar "$BT/lib/apksigner.jar" sign \
    --ks "$KS" \
    --ks-key-alias sudoku \
    --ks-pass pass:sudoku123 \
    --key-pass pass:sudoku123 \
    --min-sdk-version $MIN_SDK \
    --v1-signing-enabled true \
    --v2-signing-enabled true \
    --v3-signing-enabled true \
    --out "$DIST/Sudoku.apk" \
    "$OUT/aligned.apk"

# ---------------------------------------------------------------- 验收

echo
echo "==> 校验签名"
"$JAVA" -jar "$BT/lib/apksigner.jar" verify --print-certs "$DIST/Sudoku.apk" \
    | sed 's/^/    /'

echo
echo "==> badging"
"$AAPT2" dump badging "$DIST/Sudoku.apk" 2>/dev/null \
    | grep -E "^(package|sdkVersion|targetSdkVersion|application-label|launchable-activity|application-icon-640|native-code)" \
    | sed 's/^/    /'

SIZE=$(stat -c %s "$DIST/Sudoku.apk")
echo
echo "APK: $DIST/Sudoku.apk"
echo "大小: $SIZE 字节 ($(awk "BEGIN{printf \"%.2f\", $SIZE/1048576}") MB)"
echo "BUILD_OK"
