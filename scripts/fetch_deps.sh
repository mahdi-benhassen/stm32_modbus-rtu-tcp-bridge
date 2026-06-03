#!/usr/bin/env bash
# ============================================================
# fetch_deps.sh
#
# Downloads external dependencies for the STM32F407 Modbus
# Bridge firmware via release tarballs (faster + more reliable
# in CI than shallow git clones with tags).
#
# Dependencies:
#   - STM32CubeF4 v1.28.1   (HAL + CMSIS)
#   - FreeRTOS-Kernel V10.6.2
#   - lwIP STABLE-2_2_0_RELEASE
#
# Requires: curl, tar
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
DL_DIR="${ROOT_DIR}/.deps_cache"
TMPDIR=""

cleanup() {
    [ -n "${TMPDIR:-}" ] && rm -rf "$TMPDIR"
}
trap cleanup EXIT

echo "=== Fetching build dependencies ==="
mkdir -p "$DL_DIR"
TMPDIR=$(mktemp -d)

download_and_extract() {
    local name="$1"
    local url="$2"
    local cache_dir="$3"
    local strip_components="${4:-1}"

    if [ -d "${cache_dir}" ] && [ "$(ls -A "${cache_dir}" 2>/dev/null)" ]; then
        echo "[${name}] already cached at ${cache_dir}, skipping download"
        return 0
    fi

    local archive="${DL_DIR}/${name}.tar.gz"
    if [ ! -f "${archive}" ]; then
        echo "[${name}] downloading..."
        curl -fsSL -o "${archive}" "${url}"
    fi

    echo "[${name}] extracting..."
    mkdir -p "${cache_dir}"
    tar -xzf "${archive}" -C "${TMPDIR}" --strip-components="${strip_components}"
    mv "${TMPDIR}"/* "${cache_dir}/" 2>/dev/null || true
    rm -f "${archive}"
}

# ----------------------------------------------------------
# 1. STM32CubeF4 (CMSIS + HAL Drivers)
# ----------------------------------------------------------
CUBE_VER="v1.28.1"
CUBE_CACHE="${DL_DIR}/STM32CubeF4-${CUBE_VER}"
CUBE_URL="https://github.com/STMicroelectronics/STM32CubeF4/archive/refs/tags/${CUBE_VER}.tar.gz"

download_and_extract "STM32CubeF4" "${CUBE_URL}" "${CUBE_CACHE}" 1

echo "Copying CMSIS headers..."
CMSIS_DST="${ROOT_DIR}/Drivers/CMSIS/Include"
mkdir -p "$CMSIS_DST"
# Common CMSIS headers
cp -r "${CUBE_CACHE}/Drivers/CMSIS/Include/." "$CMSIS_DST/" 2>/dev/null || true
# Core CMSIS headers (core_cm4.h, cmsis_gcc.h, etc.)
if [ -d "${CUBE_CACHE}/Drivers/CMSIS/Core/Include" ]; then
    cp -r "${CUBE_CACHE}/Drivers/CMSIS/Core/Include/." "$CMSIS_DST/" 2>/dev/null || true
fi

# Device-specific CMSIS headers
CMSIS_DEV_DST="${ROOT_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
mkdir -p "$CMSIS_DEV_DST"
if [ -d "${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include" ]; then
    cp -r "${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include/." "$CMSIS_DEV_DST/"
fi

echo "Copying HAL drivers..."
HAL_DST="${ROOT_DIR}/Drivers/STM32F4xx_HAL_Driver"
mkdir -p "${HAL_DST}/Inc" "${HAL_DST}/Src"

HAL_SRC=(
    stm32f4xx_hal.c        stm32f4xx_hal_cortex.c   stm32f4xx_hal_dma.c
    stm32f4xx_hal_eth.c     stm32f4xx_hal_flash.c    stm32f4xx_hal_flash_ex.c
    stm32f4xx_hal_gpio.c    stm32f4xx_hal_pwr.c      stm32f4xx_hal_pwr_ex.c
    stm32f4xx_hal_rcc.c     stm32f4xx_hal_rcc_ex.c   stm32f4xx_hal_tim.c
    stm32f4xx_hal_tim_ex.c  stm32f4xx_hal_uart.c
)
HAL_SRC_PATH="${CUBE_CACHE}/Drivers/STM32F4xx_HAL_Driver/Src"
for f in "${HAL_SRC[@]}"; do
    [ -f "${HAL_SRC_PATH}/${f}" ] && cp "${HAL_SRC_PATH}/${f}" "${HAL_DST}/Src/"
done

HAL_INC=(
    stm32f4xx_hal.h         stm32f4xx_hal_cortex.h   stm32f4xx_hal_dma.h
    stm32f4xx_hal_eth.h     stm32f4xx_hal_flash.h    stm32f4xx_hal_flash_ex.h
    stm32f4xx_hal_gpio.h    stm32f4xx_hal_pwr.h      stm32f4xx_hal_pwr_ex.h
    stm32f4xx_hal_rcc.h     stm32f4xx_hal_rcc_ex.h   stm32f4xx_hal_tim.h
    stm32f4xx_hal_tim_ex.h  stm32f4xx_hal_uart.h     stm32f4xx_hal_def.h
)
HAL_INC_PATH="${CUBE_CACHE}/Drivers/STM32F4xx_HAL_Driver/Inc"
for f in "${HAL_INC[@]}"; do
    [ -f "${HAL_INC_PATH}/${f}" ] && cp "${HAL_INC_PATH}/${f}" "${HAL_DST}/Inc/"
done

# Verify critical files exist
if [ ! -f "${HAL_DST}/Inc/stm32f4xx_hal.h" ]; then
    echo "ERROR: stm32f4xx_hal.h not found! CubeF4 extraction may have failed."
    echo "Contents of ${CUBE_CACHE}:"
    ls -la "${CUBE_CACHE}/" 2>/dev/null || echo "(empty)"
    echo "Contents of Drivers if any:"
    ls -la "${CUBE_CACHE}/Drivers/" 2>/dev/null || echo "(no Drivers dir)"
    exit 1
fi

# ----------------------------------------------------------
# 2. FreeRTOS Kernel
# ----------------------------------------------------------
FRTOS_VER="V10.6.2"
FRTOS_CACHE="${DL_DIR}/FreeRTOS-Kernel-${FRTOS_VER}"
FRTOS_URL="https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/${FRTOS_VER}.tar.gz"

download_and_extract "FreeRTOS" "${FRTOS_URL}" "${FRTOS_CACHE}" 1

FRTOS_DST="${ROOT_DIR}/Middlewares/Third_Party/FreeRTOS/Source"
mkdir -p "${FRTOS_DST}/include"

# .c sources from root
cp "${FRTOS_CACHE}/"*.c "${FRTOS_DST}/" 2>/dev/null || true

# Headers from include/
if [ -d "${FRTOS_CACHE}/include" ]; then
    cp -r "${FRTOS_CACHE}/include/." "${FRTOS_DST}/include/"
else
    # Older layout: headers in root
    cp "${FRTOS_CACHE}/"*.h "${FRTOS_DST}/include/" 2>/dev/null || true
fi

# Portable layer (GCC/ARM_CM4F)
FRTOS_PORT_DST="${FRTOS_DST}/portable/GCC/ARM_CM4F"
mkdir -p "$FRTOS_PORT_DST"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/port.c" "$FRTOS_PORT_DST/"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/portmacro.h" "$FRTOS_PORT_DST/" 2>/dev/null || true

# Memory manager (heap_4)
FRTOS_MEM_DST="${FRTOS_DST}/portable/MemMang"
mkdir -p "$FRTOS_MEM_DST"
cp "${FRTOS_CACHE}/portable/MemMang/heap_4.c" "$FRTOS_MEM_DST/"

# ----------------------------------------------------------
# 3. lwIP
# ----------------------------------------------------------
LWIP_VER="STABLE-2_2_0_RELEASE"
LWIP_CACHE="${DL_DIR}/lwip-${LWIP_VER}"
LWIP_URL="https://github.com/lwip-tcpip/lwip/archive/refs/tags/${LWIP_VER}.tar.gz"

download_and_extract "lwIP" "${LWIP_URL}" "${LWIP_CACHE}" 1

LWIP_DST="${ROOT_DIR}/Middlewares/Third_Party/lwIP/src"
mkdir -p "${LWIP_DST}/include/lwip/prot"
mkdir -p "${LWIP_DST}/include/netif"

# Core
mkdir -p "${LWIP_DST}/core/ipv4"
cp "${LWIP_CACHE}/src/core/"*.c "${LWIP_DST}/core/" 2>/dev/null || true
cp "${LWIP_CACHE}/src/core/ipv4/"*.c "${LWIP_DST}/core/ipv4/" 2>/dev/null || true

# API
mkdir -p "${LWIP_DST}/api"
cp "${LWIP_CACHE}/src/api/"*.c "${LWIP_DST}/api/" 2>/dev/null || true

# Netif
mkdir -p "${LWIP_DST}/netif"
cp "${LWIP_CACHE}/src/netif/ethernet.c" "${LWIP_DST}/netif/" 2>/dev/null || true

# Headers
cp "${LWIP_CACHE}/src/include/lwip/"*.h "${LWIP_DST}/include/lwip/" 2>/dev/null || true
cp "${LWIP_CACHE}/src/include/lwip/prot/"*.h "${LWIP_DST}/include/lwip/prot/" 2>/dev/null || true
cp "${LWIP_CACHE}/src/include/netif/"*.h "${LWIP_DST}/include/netif/" 2>/dev/null || true

# Copy any stray top-level headers
cp "${LWIP_CACHE}/src/include/"*.h "${LWIP_DST}/include/" 2>/dev/null || true

echo ""
echo "=== All dependencies ready ==="
