#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

uint32_t util_utf8_decode(const char *str, int32_t offset, int32_t *bytesRead);

#endif // UTILS_H