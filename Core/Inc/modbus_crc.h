#ifndef __MODBUS_CRC_H
#define __MODBUS_CRC_H

#include <stdint.h>
#include <stddef.h>

uint16_t modbus_crc16(const uint8_t *data, uint16_t len);

#endif /* __MODBUS_CRC_H */
