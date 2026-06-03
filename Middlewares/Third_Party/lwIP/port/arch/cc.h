#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <sys/errno.h>

/* lwIP basic types */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;

typedef uintptr_t mem_ptr_t;

/* Compiler hints */
#define LWIP_NO_STDINT_H  0
#define LWIP_NO_LIMITS_H  0
#define LWIP_NO_INTTYPES_H 0

/* printf formatters for lwIP types */
#define U16_F  "hu"
#define S16_F  "hd"
#define X16_F  "hx"
#define U32_F  "lu"
#define S32_F  "ld"
#define X32_F  "lx"
#define SZT_F  "lu"

/* Byte order (STM32F4 is little-endian) */
#ifndef BYTE_ORDER
#define BYTE_ORDER  LITTLE_ENDIAN
#endif

/* Packing */
#define PACK_STRUCT_STRUCT  __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Platform diagnostics */
#define LWIP_PLATFORM_DIAG(x)   do { /* no-op */ } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { if (!(x)) { while(1); } } while(0)

/* Random (not used with LWIP_RAND=0) */
#define LWIP_RAND() 0

#endif /* LWIP_ARCH_CC_H */
