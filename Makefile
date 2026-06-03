# ============================================================
#  STM32F407VGT6 - Modbus RTU/TCP Bridge
#  Toolchain: GNU Arm Embedded (arm-none-eabi-gcc)
# ============================================================

TARGET       := modbus_bridge

# ---- Directories ----
CORE_DIR     := Core
DRV_DIR      := Drivers
MID_DIR      := Middlewares

# ---- Toolchain ----
PREFIX       := arm-none-eabi-
CC           := $(PREFIX)gcc
AS           := $(PREFIX)gcc -c
CP           := $(PREFIX)objcopy
SZ           := $(PREFIX)size
DBG          := $(PREFIX)gdb
HEX          := $(CP) -O ihex
BIN          := $(CP) -O binary -S

# ---- MCU Flags ----
CPU          := -mcpu=cortex-m4
FPU          := -mfpu=fpv4-sp-d16 -mfloat-abi=hard
MCU_FLAGS    := $(CPU) -mthumb $(FPU)

# ---- Compiler Flags ----
C_DEFS       := -DUSE_HAL_DRIVER -DSTM32F407xx
C_INCLUDES   := -I$(CORE_DIR)/Inc \
                -I$(DRV_DIR)/CMSIS/Include \
                -I$(DRV_DIR)/CMSIS/Device/ST/STM32F4xx/Include \
                -I$(DRV_DIR)/STM32F4xx_HAL_Driver/Inc \
                -I$(MID_DIR)/Third_Party/FreeRTOS/Source/include \
                -I$(MID_DIR)/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
                -I$(MID_DIR)/Third_Party/lwIP/src/include \
                -I$(MID_DIR)/Third_Party/lwIP/src/include/lwip \
                -I$(MID_DIR)/Third_Party/lwIP/src/include/netif \
                -I$(MID_DIR)/Third_Party/lwIP/port

CFLAGS       := $(MCU_FLAGS) $(C_DEFS) $(C_INCLUDES) \
                -Og -g -Wall -Wextra -Wpedantic \
                -ffunction-sections -fdata-sections \
                -fno-common -fmessage-length=0 \
                -fsingle-precision-constant \
                -std=gnu11

LDFLAGS      := $(MCU_FLAGS) -Tstm32f407_flash.ld \
                -Wl,-Map=$(TARGET).map,--cref \
                -Wl,--gc-sections \
                -specs=nano.specs -specs=nosys.specs \
                -Wl,--undefined=uxTopUsedPriority

# ---- Source Files ----
C_SOURCES    := $(CORE_DIR)/Src/main.c \
                $(CORE_DIR)/Src/stm32f4xx_it.c \
                $(CORE_DIR)/Src/system_stm32f4xx.c \
                $(CORE_DIR)/Src/syscalls.c \
                $(CORE_DIR)/Src/sys_arch.c \
                $(CORE_DIR)/Src/modbus_crc.c \
                $(CORE_DIR)/Src/rs485_driver.c \
                $(CORE_DIR)/Src/tcp_server.c \
                $(CORE_DIR)/Src/bridge_engine.c \
                $(CORE_DIR)/Src/ethernetif.c

# ---- HAL Driver Sources (add real paths from STM32CubeF4) ----
C_SOURCES    += $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_eth.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
                $(DRV_DIR)/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c

# ---- FreeRTOS Sources ----
C_SOURCES    += $(MID_DIR)/Third_Party/FreeRTOS/Source/croutine.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/event_groups.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/list.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/queue.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/stream_buffer.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/tasks.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/timers.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
                $(MID_DIR)/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c

# ---- lwIP Sources ----
LWIP_DIR      := $(MID_DIR)/Third_Party/lwIP/src
C_SOURCES    += $(LWIP_DIR)/core/init.c \
                $(LWIP_DIR)/core/def.c \
                $(LWIP_DIR)/core/dns.c \
                $(LWIP_DIR)/core/inet_chksum.c \
                $(LWIP_DIR)/core/ip.c \
                $(LWIP_DIR)/core/mem.c \
                $(LWIP_DIR)/core/memp.c \
                $(LWIP_DIR)/core/netif.c \
                $(LWIP_DIR)/core/pbuf.c \
                $(LWIP_DIR)/core/raw.c \
                $(LWIP_DIR)/core/stats.c \
                $(LWIP_DIR)/core/sys.c \
                $(LWIP_DIR)/core/tcp.c \
                $(LWIP_DIR)/core/tcp_in.c \
                $(LWIP_DIR)/core/tcp_out.c \
                $(LWIP_DIR)/core/timeouts.c \
                $(LWIP_DIR)/core/udp.c \
                $(LWIP_DIR)/core/ipv4/autoip.c \
                $(LWIP_DIR)/core/ipv4/dhcp.c \
                $(LWIP_DIR)/core/ipv4/etharp.c \
                $(LWIP_DIR)/core/ipv4/icmp.c \
                $(LWIP_DIR)/core/ipv4/igmp.c \
                $(LWIP_DIR)/core/ipv4/ip4.c \
                $(LWIP_DIR)/core/ipv4/ip4_addr.c \
                $(LWIP_DIR)/core/ipv4/ip4_frag.c \
                $(LWIP_DIR)/netif/ethernet.c \
                $(LWIP_DIR)/api/api_lib.c \
                $(LWIP_DIR)/api/api_msg.c \
                $(LWIP_DIR)/api/err.c \
                $(LWIP_DIR)/api/if_api.c \
                $(LWIP_DIR)/api/netbuf.c \
                $(LWIP_DIR)/api/netdb.c \
                $(LWIP_DIR)/api/netifapi.c \
                $(LWIP_DIR)/api/sockets.c \
                $(LWIP_DIR)/api/tcpip.c

# ---- Startup File ----
ASM_SOURCES  := startup_stm32f407xx.s

# ---- Object Files ----
OBJECTS      := $(C_SOURCES:.c=.o)
OBJECTS      += $(ASM_SOURCES:.s=.o)

# ---- Rules ----
.PHONY: all clean flash

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin

%.o: %.c
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) $< -o $@

%.o: %.s
	@echo "AS $<"
	@$(AS) -c $(CFLAGS) $< -o $@

$(TARGET).elf: $(OBJECTS)
	@echo "LD $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@

$(TARGET).hex: $(TARGET).elf
	$(HEX) $< $@

$(TARGET).bin: $(TARGET).elf
	$(BIN) $< $@

clean:
	rm -f $(OBJECTS) $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map

flash: $(TARGET).bin
	openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
		-c "program $(TARGET).bin 0x08000000 verify reset exit"

debug:
	$(DBG) $(TARGET).elf
