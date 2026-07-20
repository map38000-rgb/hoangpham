name: Build Android ARM64 Native Hook

on:
  push:
    branches: [ "main", "master" ]
  pull_request:
    branches: [ "main", "master" ]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-latest
    # Sử dụng môi trường Docker chuyên dụng chứa sẵn Android SDK/NDK chuẩn hóa
    container:
      image: mingc/android-ndk:r25c

    steps:
    - name: Checkout Code
      uses: actions/checkout@v4

    # Cài đặt git và cmake phiên bản container nếu chưa có
    - name: Install System Dependencies
      run: |
        apt-get update && apt-get install -y git cmake make ninja-build

    - name: Download Dependencies (Dobby & XDL)
      run: |
        echo "[*] Cleaning old directories if any..."
        rm -rf dobby xdl
        
        echo "[*] Downloading Dobby Hook Framework..."
        git clone --depth=1 https://github.com/jmpews/Dobby.git dobby
        
        echo "[*] Downloading xdl (Linker Extension)..."
        git clone --depth=1 https://github.com/hexhacking/xdl.git xdl

    # Tự động quét tìm vị trí chính xác của file xdl.c
    - name: Locate XDL Files Dynamically
      run: |
        XDL_FILE_PATH=$(find $(pwd)/xdl -name "xdl.c" | head -n 1)
        XDL_DIR_PATH=$(dirname "$XDL_FILE_PATH")
        echo "REAL_XDL_SRC=$XDL_FILE_PATH" >> $GITHUB_ENV
        echo "REAL_XDL_INC=$XDL_DIR_PATH" >> $GITHUB_ENV

    # Biên dịch trong môi trường Android thuần khiết
    - name: Build Main Hook Library
      run: |
        mkdir -p build && cd build
        cmake .. \
          -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
          -DANDROID_ABI=arm64-v8a \
          -DANDROID_PLATFORM=android-24 \
          -DDT_XDL_SRC="$REAL_XDL_SRC" \
          -DDT_XDL_INC="$REAL_XDL_INC" \
          -DCMAKE_BUILD_TYPE=Release
        make -j$(nproc)

    - name: Upload Output Library (.so)
      uses: actions/upload-artifact@v4
      with:
        name: libebpf_helper-arm64
        path: build/libebpf_helper.so
