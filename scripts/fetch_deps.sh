#!/usr/bin/env bash
# ============================================================
# fetch_deps.sh
#
# Downloads/fetches the external dependencies required to
# build the STM32F407 Modbus Bridge firmware:
#   - STM32CubeF4 (HAL + CMSIS)
#   - FreeRTOS Kernel
#   - lwIP stack
#
# Run this once before building, or let the CI workflow
# call it automatically.
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

# ----------------------------------------------------------
# 1. STM32CubeF4 (HAL Drivers + CMSIS)
#    Source: STMicroelectronics GitHub mirror
# ----------------------------------------------------------
CUBE_REF="v1.28.1"
CUBE_CACHE="${DL_DIR}/STM32CubeF4-${CUBE_REF}"

if [ ! -d "${CUBE_CACHE}" ]; then
    echo "Cloning STM32CubeF4 ${CUBE_REF} (shallow)..."
    git clone --depth 1 --branch "${CUBE_REF}" \
        https://github.com/STMicroelectronics/STM32CubeF4.git \
        "${CUBE_CACHE}"
else
    echo "STM32CubeF4 already cached at ${CUBE_CACHE}"
fi

echo "Copying CMSIS headers..."
CMSIS_DST="${ROOT_DIR}/Drivers/CMSIS/Include"
mkdir -p "$CMSIS_DST"
cp -r "${CUBE_CACHE}/Drivers/CMSIS/Include/." "$CMSIS_DST/"
# Copy device-specific headers
CMSIS_DEV_DST="${ROOT_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
mkdir -p "$CMSIS_DEV_DST"
cp -r "${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include/." "$CMSIS_DEV_DST" 2>/dev/null || true
# The main CMSIS device header for F4
DEVICE_H="${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f407xx.h"
if [ -f "$DEVICE_H" ]; then
    cp "$DEVICE_H" "$CMSIS_DST/"
fi
# system_stm32f4xx.h
SYS_H="${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include/system_stm32f4xx.h"
[ -f "$SYS_H" ] && cp "$SYS_H" "$CMSIS_DST/"

echo "Copying HAL drivers..."
HAL_DST="${ROOT_DIR}/Drivers/STM32F4xx_HAL_Driver"
mkdir -p "${HAL_DST}/Inc" "${HAL_DST}/Src"

# All HAL source files needed
HAL_FILES=(
    stm32f4xx_hal.c        stm32f4xx_hal_cortex.c   stm32f4xx_hal_dma.c
    stm32f4xx_hal_eth.c     stm32f4xx_hal_flash.c    stm32f4xx_hal_flash_ex.c
    stm32f4xx_hal_gpio.c    stm32f4xx_hal_pwr.c      stm32f4xx_hal_pwr_ex.c
    stm32f4xx_hal_rcc.c     stm32f4xx_hal_rcc_ex.c   stm32f4xx_hal_tim.c
    stm32f4xx_hal_tim_ex.c  stm32f4xx_hal_uart.c
)
for f in "${HAL_FILES[@]}"; do
    cp "${CUBE_CACHE}/Drivers/STM32F4xx_HAL_Driver/Src/${f}" "${HAL_DST}/Src/" 2>/dev/null || true
done

HAL_HEADS=(
    stm32f4xx_hal.h         stm32f4xx_hal_cortex.h   stm32f4xx_hal_dma.h
    stm32f4xx_hal_eth.h     stm32f4xx_hal_flash.h    stm32f4xx_hal_flash_ex.h
    stm32f4xx_hal_gpio.h    stm32f4xx_hal_pwr.h      stm32f4xx_hal_pwr_ex.h
    stm32f4xx_hal_rcc.h     stm32f4xx_hal_rcc_ex.h   stm32f4xx_hal_tim.h
    stm32f4xx_hal_tim_ex.h  stm32f4xx_hal_uart.h     stm32f4xx_hal_def.h
    stm32f407xx.h           system_stm32f4xx.h       stm32f4xx_ll_utils.h
)
for f in "${HAL_HEADS[@]}"; do
    cp "${CUBE_CACHE}/Drivers/STM32F4xx_HAL_Driver/Inc/${f}" "${HAL_DST}/Inc/" 2>/dev/null || true
    # Also check in CMSIS Device Include
    cp "${CUBE_CACHE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include/${f}" "${HAL_DST}/Inc/" 2>/dev/null || true
done

# Legacy stm32f4xx_hal_conf_template.h -> stm32f4xx_hal_conf.h
LEGACY_CONF="${CUBE_CACHE}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy/stm32f4xx_hal_conf.h"
if [ -f "$LEGACY_CONF" ]; then
    cp "$LEGACY_CONF" "${HAL_DST}/Inc/"
fi

# ----------------------------------------------------------
# 2. FreeRTOS Kernel (v10.6.2)
# ----------------------------------------------------------
FRTOS_REF="V10.6.2"
FRTOS_CACHE="${DL_DIR}/FreeRTOS-Kernel-${FRTOS_REF}"

if [ ! -d "${FRTOS_CACHE}" ]; then
    echo "Cloning FreeRTOS-Kernel ${FRTOS_REF} (shallow)..."
    git clone --depth 1 --branch "${FRTOS_REF}" \
        https://github.com/FreeRTOS/FreeRTOS-Kernel.git \
        "${FRTOS_CACHE}"
else
    echo "FreeRTOS already cached at ${FRTOS_CACHE}"
fi

FRTOS_DST="${ROOT_DIR}/Middlewares/Third_Party/FreeRTOS/Source"
mkdir -p "${FRTOS_DST}/include"
cp "${FRTOS_CACHE}/include/." "${FRTOS_DST}/include/" -r
cp "${FRTOS_CACHE}/"*.c "${FRTOS_DST}/"
cp "${FRTOS_CACHE}/"*.h "${FRTOS_DST}/" 2>/dev/null || true

# Copy portable layer (GCC/ARM_CM4F)
FRTOS_PORT_DST="${FRTOS_DST}/portable/GCC/ARM_CM4F"
mkdir -p "$FRTOS_PORT_DST"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/port.c" "$FRTOS_PORT_DST/"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/portmacro.h" "$FRTOS_PORT_DST/"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/portmacrocommon.h" "$FRTOS_PORT_DST/" 2>/dev/null || true

# Copy memory managers
FRTOS_MEM_DST="${FRTOS_DST}/portable/MemMang"
mkdir -p "$FRTOS_MEM_DST"
cp "${FRTOS_CACHE}/portable/MemMang/heap_4.c" "$FRTOS_MEM_DST/"

# ----------------------------------------------------------
# 3. lwIP (v2.2.0) — cloned from official GitHub mirror
# ----------------------------------------------------------
LWIP_REF="STABLE-2_2_0_RELEASE"
LWIP_CACHE="${DL_DIR}/lwip-${LWIP_REF}"

if [ ! -d "${LWIP_CACHE}" ]; then
    echo "Cloning lwIP ${LWIP_REF} (shallow)..."
    git clone --depth 1 --branch "${LWIP_REF}" \
        https://github.com/lwip-tcpip/lwip.git \
        "${LWIP_CACHE}"
else
    echo "lwIP already cached at ${LWIP_CACHE}"
fi

LWIP_DST="${ROOT_DIR}/Middlewares/Third_Party/lwIP/src"
mkdir -p "${LWIP_DST}/include/lwip" "${LWIP_DST}/include/netif"

# Core
LWIP_CORE_DST="${LWIP_DST}/core"
mkdir -p "$LWIP_CORE_DST"
cp "${LWIP_CACHE}/src/core/"*.c "$LWIP_CORE_DST/"
mkdir -p "${LWIP_CORE_DST}/ipv4"
cp "${LWIP_CACHE}/src/core/ipv4/"*.c "${LWIP_CORE_DST}/ipv4/"

# API
LWIP_API_DST="${LWIP_DST}/api"
mkdir -p "$LWIP_API_DST"
cp "${LWIP_CACHE}/src/api/"*.c "$LWIP_API_DST/"

# Netif
LWIP_NETIF_DST="${LWIP_DST}/netif"
mkdir -p "$LWIP_NETIF_DST"
cp "${LWIP_CACHE}/src/netif/ethernet.c" "$LWIP_NETIF_DST/"

# Headers
cp "${LWIP_CACHE}/src/include/lwip/"*.h "${LWIP_DST}/include/lwip/"
cp "${LWIP_CACHE}/src/include/netif/"*.h "${LWIP_DST}/include/netif/"
# Prot headers
LWIP_PROT_DST="${LWIP_DST}/include/lwip/prot"
mkdir -p "$LWIP_PROT_DST"
cp "${LWIP_CACHE}/src/include/lwip/prot/"*.h "$LWIP_PROT_DST/" 2>/dev/null || true

# lwipopts.h - custom, already in Core/Inc; skip

echo ""
echo "=== Dependencies ready ==="
echo "You can now run: make"
