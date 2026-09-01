#!/usr/bin/env bash
set -e

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}${BOLD}[+] UltraClock Android APK Build Utility${NC}\n"

SDK_DIR="${ANDROID_HOME:-$HOME/.android/sdk}"
mkdir -p "$SDK_DIR"

if [ ! -d "$SDK_DIR/cmdline-tools" ]; then
    echo -e "${YELLOW}[i] Android SDK not detected at $SDK_DIR.${NC}"
    echo -e "${CYAN}[+] Downloading Android Command Line Tools...${NC}"
    curl -fsSL https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -o /tmp/cmdline-tools.zip
    unzip -q /tmp/cmdline-tools.zip -d /tmp/cmdline-tools-raw
    mkdir -p "$SDK_DIR/cmdline-tools/latest"
    mv /tmp/cmdline-tools-raw/cmdline-tools/* "$SDK_DIR/cmdline-tools/latest/"
    rm -rf /tmp/cmdline-tools.zip /tmp/cmdline-tools-raw
fi

echo -e "${CYAN}[+] Installing Android SDK platforms & NDK...${NC}"
yes | "$SDK_DIR/cmdline-tools/latest/bin/sdkmanager" --licenses > /dev/null 2>&1 || true
"$SDK_DIR/cmdline-tools/latest/bin/sdkmanager" "platforms;android-34" "build-tools;34.0.0" "ndk;25.2.9519653"

echo -e "${GREEN}[✓] Android SDK & NDK setup complete!${NC}"
echo -e "${CYAN}[+] Building UltraClock APK...${NC}"

export ANDROID_HOME="$SDK_DIR"
export ANDROID_NDK_HOME="$SDK_DIR/ndk/25.2.9519653"

cd "$(dirname "$0")"
if [ -f "./gradlew" ]; then
    ./gradlew assembleRelease
else
    echo -e "${YELLOW}[i] Run gradle assembleRelease or open android/ directory in Android Studio.${NC}"
fi

echo -e "${GREEN}[✓] APK build script ready!${NC}"
