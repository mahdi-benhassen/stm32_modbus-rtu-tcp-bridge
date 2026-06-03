#include "modbus_crc.h"

/*
 * Modbus CRC-16 (CRC-16/MODBUS)
 * Polynomial: 0x8005 (reversed 0xA001)
 * Initial value: 0xFFFF
 * No XOR out, no reflection
 */
uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}
