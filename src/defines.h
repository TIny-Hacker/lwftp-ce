#ifndef DEFINES_H
#define DEFINES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWIP_MAX_HEAP       (1024 * 32)

#define IP_WAIT_TICKS       3000u

#define MAX_INPUT_LENGTH    32u

#define INPUT_DEFAULT       0
#define INPUT_UPPER         1
#define INPUT_LOWER         2

#define FTP_REMOTE_PATH "lwftp-test.txt"

struct preferences_t {
    uint8_t bgColor;
    uint8_t fgColor;
    uint8_t hlColor;
    uint8_t textColor;
    uint8_t client[4];
    uint8_t mask[4];
    uint8_t gw[4];
};

extern struct preferences_t prefs;

#ifdef __cplusplus
}
#endif

#endif
