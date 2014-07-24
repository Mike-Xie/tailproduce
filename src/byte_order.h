// Convert integers into big endian (MSB) format.
// Used for lexicographically correct order of time keys when using them as storage keys.

#ifndef TAILPRODUCE_BYTE_ORDER_H
#define TAILPRODUCE_BYTE_ORDER_H

#ifdef __APPLE__
#include "endian.h"
#else
#include <endian.h>
#endif

template <typename T> T as_msb(T);

template <> uint8_t as_msb(uint8_t x) {
    return x;
}
template <> int8_t as_msb(int8_t x) {
    return x;
}

template <> uint16_t as_msb(uint16_t x) {
    return htobe16(x);
}
template <> int16_t as_msb(int16_t x) {
    return htobe16(x);
}

template <> uint32_t as_msb(uint32_t x) {
    return htobe32(x);
}
template <> int32_t as_msb(int32_t x) {
    return htobe32(x);
}

template <> uint64_t as_msb(uint64_t x) {
    return htobe64(x);
}
template <> int64_t as_msb(int64_t x) {
    return htobe64(x);
}

template <typename T> T msb_to_host_order(T);

template <> uint8_t msb_to_host_order(uint8_t x) {
    return x;
}
template <> int8_t msb_to_host_order(int8_t x) {
    return x;
}

template <> uint16_t msb_to_host_order(uint16_t x) {
    return be16toh(x);
}
template <> int16_t msb_to_host_order(int16_t x) {
    return be16toh(x);
}

template <> uint32_t msb_to_host_order(uint32_t x) {
    return be32toh(x);
}
template <> int32_t msb_to_host_order(int32_t x) {
    return be32toh(x);
}

template <> uint64_t msb_to_host_order(uint64_t x) {
    return be64toh(x);
}
template <> int64_t msb_to_host_order(int64_t x) {
    return be64toh(x);
}

#endif  // TAILPRODUCE_BYTE_ORDER_H
