#!/bin/bash
set -e

CURRENT_DIR=`pwd`

# MTEngineSDL
cd $CURRENT_DIR/../
if [ ! -d "MTEngineSDL" ]; then
	echo -e "\e[94mCloning \e[31mMTEngineSDL \e[94mlibrary repository\e[0m"
	git clone --recursive https://github.com/slajerek/MTEngineSDL.git
	echo -e ""
else
	cd MTEngineSDL
	git submodule update --init --recursive
	cd ..
fi

echo -e "\e[94mCompiling \e[31mMTEngineSDL \e[94mlibrary\e[0m"
mkdir -p $CURRENT_DIR/../MTEngineSDL/build
cd $CURRENT_DIR/../MTEngineSDL/build

# ggml/llama.cpp: on ARM the -mcpu=native dotprod auto-detection can emit `sdot`
# instructions the assembler rejects ("selected processor does not support sdot").
# Disable native CPU optimizations on ARM (baseline armv8-a); keep native on x86_64.
GGML_ARCH_ARGS=""
case "$(uname -m)" in
	aarch64|arm64)
		echo -e "\e[94mARM detected - building ggml without native CPU optimizations (MT_GGML_NATIVE=OFF)\e[0m"
		GGML_ARCH_ARGS="-DMT_GGML_NATIVE=OFF"
		;;
esac

# Always (re)run cmake so flag changes (e.g. MT_GGML_NATIVE) actually take effect
# even when a build dir already exists. cmake is idempotent and cheap.
cmake ../ -DMT_ENABLE_MBEDTLS=OFF ${GGML_ARCH_ARGS} ${CMAKE_EXTRA_ARGS}
make -j$(nproc) MTEngineSDL

# uSockets
cd $CURRENT_DIR/../
if [ ! -d "uSockets" ]; then
	echo -e "\n\e[94mCloning \e[31muSockets \e[94mlibrary repository\e[0m"
	git clone https://github.com/uNetworking/uSockets.git
fi

echo -e "\n\e[94mCompiling \e[31muSockets \e[94mlibrary\e[0m"
cd uSockets
make -j$(nproc)
mkdir -p $CURRENT_DIR/../MTEngineSDL/platform/Linux/libs/
cp -f uSockets.a $CURRENT_DIR/../MTEngineSDL/platform/Linux/libs/

# RetroDebugger
echo -e "\n\e[94mCompiling \e[31mRetroDebugger\e[0m"
mkdir -p $CURRENT_DIR/build
cd $CURRENT_DIR/build
cmake ../
make -j$(nproc) retrodebugger

echo -e "\n\e[1;92mRetroDebugger compiled successfully. Binary is in ./build folder.\e[0m"
