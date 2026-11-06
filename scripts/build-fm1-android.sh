#!/bin/sh
# Groovebox — OFAD — AGPL-3.0-or-later
# v0.49 : émulateur FM-1 (Rust) pour Android, avant Projucer / Gradle : bibliothèques statiques dans
# third_party/fm1emu/android/<ABI>/libfm1_bridge.a (lues par android/Groovebox.jucer).
# Usage : scripts/build-fm1-android.sh [dossier NDK]   (Rust : rustup target add aarch64-linux-android armv7-linux-androideabi)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${1:-${ANDROID_NDK_HOME:-$(ls -d /opt/android-sdk/ndk/* 2>/dev/null | sort | tail -1)}}
HOST=$(ls "$NDK/toolchains/llvm/prebuilt" | head -1)
TC="$NDK/toolchains/llvm/prebuilt/$HOST/bin"
export AR="$TC/llvm-ar"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$TC/aarch64-linux-android26-clang"
export CARGO_TARGET_ARMV7_LINUX_ANDROIDEABI_LINKER="$TC/armv7a-linux-androideabi26-clang"
export CARGO_TARGET_DIR="$ROOT/third_party/fm1emu/target"
cd "$ROOT/third_party/fm1emu"
for pair in aarch64-linux-android:arm64-v8a armv7-linux-androideabi:armeabi-v7a; do
  t=${pair%%:*}; abi=${pair##*:}
  cargo build --release --locked -p fm1-bridge --target "$t"
  mkdir -p "android/$abi"
  cp "$CARGO_TARGET_DIR/$t/release/libfm1_bridge.a" "android/$abi/libfm1_bridge.a"
done
echo "Émulateur FM-1 pour Android : third_party/fm1emu/android/{arm64-v8a,armeabi-v7a}"
