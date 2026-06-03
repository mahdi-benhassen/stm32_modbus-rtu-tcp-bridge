#!/usr/bin/env bash
# ============================================================
# fetch_deps.sh
#
# Downloads external dependencies for the STM32F407 Modbus
# Bridge firmware.
#
# STM32CubeF4 uses git submodules for HAL/CMSIS — GitHub
# release tarballs don't include submodule contents, so we
# clone the three component repos individually.
#
# Dependencies:
#   - stm32f4xx_hal_driver (HAL)     — git shallow clone
#   - cmsis_device_f4     (CMSIS dev) — git shallow clone
#   - CMSIS_5             (CMSIS core)— git sparse checkout
#   - FreeRTOS-Kernel V10.6.2        — tarball
#   - lwIP STABLE-2_2_0_RELEASE      — tarball
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
# Helper: download a release tarball via curl+tar
# ----------------------------------------------------------
download_tarball() {
    local name="$1"
    local url="$2"
    local cache_dir="$3"
    local strip="${4:-1}"

    if [ -d "${cache_dir}" ] && [ "$(ls -A "${cache_dir}" 2>/dev/null)" ]; then
        echo "[${name}] already cached, skipping download"
        return 0
    fi

    local archive="${DL_DIR}/${name}.tar.gz"
    echo "[${name}] downloading..."
    curl -fsSL -o "${archive}" "${url}"

    echo "[${name}] extracting..."
    mkdir -p "${cache_dir}"
    tar -xzf "${archive}" -C "${TMPDIR}" --strip-components="${strip}"
    cp -r "${TMPDIR}/." "${cache_dir}/" 2>/dev/null || true
    rm -rf "${TMPDIR:?}"/*
    rm -f "${archive}"
}

# ----------------------------------------------------------
# Helper: shallow git clone (default branch, depth 1)
# ----------------------------------------------------------
shallow_clone() {
    local name="$1"
    local url="$2"
    local cache_dir="$3"

    if [ -d "${cache_dir}/.git" ]; then
        echo "[${name}] already cloned, skipping"
        return 0
    fi

    echo "[${name}] cloning (shallow)..."
    git clone --depth 1 "${url}" "${cache_dir}"
}

# ----------------------------------------------------------
# 1a. STM32F4xx HAL Driver
#     Source: STMicroelectronics/stm32f4xx_hal_driver
# ----------------------------------------------------------
HAL_CACHE="${DL_DIR}/stm32f4xx_hal_driver"
shallow_clone "STM32 HAL" \
    "https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git" \
    "${HAL_CACHE}"

HAL_DST="${ROOT_DIR}/Drivers/STM32F4xx_HAL_Driver"
mkdir -p "${HAL_DST}/Inc" "${HAL_DST}/Src"
cp -r "${HAL_CACHE}/Inc/." "${HAL_DST}/Inc/" 2>/dev/null || true
cp -r "${HAL_CACHE}/Src/." "${HAL_DST}/Src/" 2>/dev/null || true

if [ ! -f "${HAL_DST}/Inc/stm32f4xx_hal.h" ]; then
    echo "ERROR: stm32f4xx_hal.h not found!"
    echo "HAL cache contents:"
    ls -la "${HAL_CACHE}/" 2>/dev/null || echo "(empty)"
    exit 1
fi
echo "  HAL driver: OK"

# ----------------------------------------------------------
# 1b. CMSIS Device F4
#     Source: STMicroelectronics/cmsis_device_f4
# ----------------------------------------------------------
CMSIS_DEV_CACHE="${DL_DIR}/cmsis_device_f4"
shallow_clone "CMSIS Device F4" \
    "https://github.com/STMicroelectronics/cmsis_device_f4.git" \
    "${CMSIS_DEV_CACHE}"

CMSIS_DEV_DST="${ROOT_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
mkdir -p "${CMSIS_DEV_DST}"
cp -r "${CMSIS_DEV_CACHE}/Include/." "${CMSIS_DEV_DST}/" 2>/dev/null || true

if [ ! -f "${CMSIS_DEV_DST}/stm32f407xx.h" ]; then
    echo "ERROR: stm32f407xx.h not found!"
    echo "CMSIS device cache contents:"
    ls -la "${CMSIS_DEV_CACHE}/" 2>/dev/null || echo "(empty)"
    exit 1
fi
echo "  CMSIS Device: OK"

# ----------------------------------------------------------
# 1c. CMSIS Core (core_cm4.h, cmsis_gcc.h, etc.)
#     Source: ARM-software/CMSIS_5  (sparse checkout for speed)
# ----------------------------------------------------------
CMSIS_CORE_CACHE="${DL_DIR}/cmsis_core"
if [ ! -d "${CMSIS_CORE_CACHE}/core_cm4.h" ]; then
    rm -rf "${CMSIS_CORE_CACHE}"
    echo "[CMSIS Core] cloning (sparse, blobless)..."

    if git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/ARM-software/CMSIS_5.git \
        "${CMSIS_CORE_CACHE}" 2>/dev/null; then
        cd "${CMSIS_CORE_CACHE}"
        git sparse-checkout set CMSIS/Core/Include
    else
        echo "[CMSIS Core] sparse clone failed, falling back to full shallow clone..."
        git clone --depth 1 \
            https://github.com/ARM-software/CMSIS_5.git \
            "${CMSIS_CORE_CACHE}"
    fi
    cd "${ROOT_DIR}"
fi

CMSIS_DST="${ROOT_DIR}/Drivers/CMSIS/Include"
mkdir -p "${CMSIS_DST}"
cp -r "${CMSIS_CORE_CACHE}/CMSIS/Core/Include/." "${CMSIS_DST}/" 2>/dev/null || true

if [ ! -f "${CMSIS_DST}/core_cm4.h" ]; then
    echo "ERROR: core_cm4.h not found!"
    echo "CMSIS core cache contents:"
    ls -la "${CMSIS_CORE_CACHE}/" 2>/dev/null || echo "(empty)"
    exit 1
fi
echo "  CMSIS Core: OK"

# ----------------------------------------------------------
# 2. FreeRTOS Kernel V10.6.2
# ----------------------------------------------------------
FRTOS_VER="V10.6.2"
FRTOS_CACHE="${DL_DIR}/FreeRTOS-Kernel-${FRTOS_VER}"
FRTOS_URL="https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/${FRTOS_VER}.tar.gz"
download_tarball "FreeRTOS" "${FRTOS_URL}" "${FRTOS_CACHE}" 1

FRTOS_DST="${ROOT_DIR}/Middlewares/Third_Party/FreeRTOS/Source"
mkdir -p "${FRTOS_DST}/include"
cp "${FRTOS_CACHE}/"*.c "${FRTOS_DST}/" 2>/dev/null || true
if [ -d "${FRTOS_CACHE}/include" ]; then
    cp -r "${FRTOS_CACHE}/include/." "${FRTOS_DST}/include/"
else
    cp "${FRTOS_CACHE}/"*.h "${FRTOS_DST}/include/" 2>/dev/null || true
fi
FRTOS_PORT_DST="${FRTOS_DST}/portable/GCC/ARM_CM4F"
mkdir -p "$FRTOS_PORT_DST"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/port.c" "$FRTOS_PORT_DST/"
cp "${FRTOS_CACHE}/portable/GCC/ARM_CM4F/portmacro.h" "$FRTOS_PORT_DST/" 2>/dev/null || true
FRTOS_MEM_DST="${FRTOS_DST}/portable/MemMang"
mkdir -p "$FRTOS_MEM_DST"
cp "${FRTOS_CACHE}/portable/MemMang/heap_4.c" "$FRTOS_MEM_DST/"
echo "  FreeRTOS: OK"

# ----------------------------------------------------------
# 3. lwIP STABLE-2_2_0_RELEASE
# ----------------------------------------------------------
LWIP_VER="STABLE-2_2_0_RELEASE"
LWIP_CACHE="${DL_DIR}/lwip-${LWIP_VER}"
LWIP_URL="https://github.com/lwip-tcpip/lwip/archive/refs/tags/${LWIP_VER}.tar.gz"
download_tarball "lwIP" "${LWIP_URL}" "${LWIP_CACHE}" 1

LWIP_DST="${ROOT_DIR}/Middlewares/Third_Party/lwIP/src"
mkdir -p "${LWIP_DST}/include/lwip/prot" "${LWIP_DST}/include/lwip/priv" "${LWIP_DST}/include/netif"
mkdir -p "${LWIP_DST}/core/ipv4" "${LWIP_DST}/api" "${LWIP_DST}/netif"
cp "${LWIP_CACHE}/src/core/"*.c         "${LWIP_DST}/core/"      2>/dev/null || true
cp "${LWIP_CACHE}/src/core/ipv4/"*.c    "${LWIP_DST}/core/ipv4/"  2>/dev/null || true
cp "${LWIP_CACHE}/src/api/"*.c          "${LWIP_DST}/api/"        2>/dev/null || true
cp "${LWIP_CACHE}/src/netif/ethernet.c"  "${LWIP_DST}/netif/"      2>/dev/null || true
cp "${LWIP_CACHE}/src/include/lwip/"*.h             "${LWIP_DST}/include/lwip/"      2>/dev/null || true
cp "${LWIP_CACHE}/src/include/lwip/prot/"*.h        "${LWIP_DST}/include/lwip/prot/"  2>/dev/null || true
cp "${LWIP_CACHE}/src/include/lwip/priv/"*.h        "${LWIP_DST}/include/lwip/priv/"  2>/dev/null || true
cp "${LWIP_CACHE}/src/include/netif/"*.h            "${LWIP_DST}/include/netif/"      2>/dev/null || true
cp "${LWIP_CACHE}/src/include/"*.h                  "${LWIP_DST}/include/"            2>/dev/null || true
echo "  lwIP: OK"

echo ""
echo "=== All dependencies ready ==="
