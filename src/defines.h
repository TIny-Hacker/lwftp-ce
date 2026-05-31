#ifndef DEFINES_H
#define DEFINES_H

#include "lwftp.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWIP_MAX_HEAP       (1024 * 32)

#define IP_WAIT_TICKS       3000

#define INPUT_BUF_SIZE      32
#define RX_BUF_SIZE         255

#define INPUT_DEFAULT       0
#define INPUT_UPPER         1
#define INPUT_LOWER         2

#define TYPE_UNKNOWN        0
#define TYPE_DIR            1
#define TYPE_PROG           2
#define TYPE_APPVAR         3

#define LOCAL_DIRTY         (1 << 0)
#define REMOTE_DIRTY        (1 << 1)
#define PATH_DIRTY          (1 << 2)
#define BUTTONS_DIRTY       (1 << 3)
#define ALL_DIRTY           (LOCAL_DIRTY | REMOTE_DIRTY | PATH_DIRTY | BUTTONS_DIRTY)

#define MAX_SHOWN_FILES     15
#define MAX_LOCAL_FILES     500
#define LOCAL_FILES         ((struct file_t *)0xD031F6)
#define MAX_REMOTE_FILES    ((8400 / sizeof(struct file_t)) - MAX_LOCAL_FILES)
#define REMOTE_FILES        (LOCAL_FILES + (sizeof(struct file_t) * MAX_LOCAL_FILES))

#define FILE_BUFFER         ((uint8_t *)0xD52C00)

struct preferences_t {
    uint8_t bgColor;
    uint8_t fgColor;
    uint8_t hlColor;
    uint8_t textColor;
    uint8_t client[4];
    uint8_t mask[4];
    uint8_t gw[4];
};

struct app_t {
    uint8_t dirty;
    bool remoteColumn;
    unsigned int start[2];
    unsigned int selected[2];
    unsigned int total[2];
    uint8_t dir;

    bool connected;
    bool ftpStarted;
    bool busy;
    int ftpResult;
    unsigned int txOffset;
    unsigned int txSize;
    char rxBuf[RX_BUF_SIZE + 2];
    char path[RX_BUF_SIZE + 2];
    unsigned int rxOffset;
};

struct file_t {
    uint8_t type;
    char name[9];
};

extern struct app_t app;
extern struct preferences_t prefs;

#ifdef __cplusplus
}
#endif

#endif
