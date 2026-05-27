#ifndef FTP_H
#define FTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ftp_ConnectCallback(void *arg, int result);

uint16_t ftp_ListDataSink(void *arg, const char *ptr, uint16_t len);

void ftp_ListCallback(void *arg, int result);

#ifdef __cplusplus
}
#endif

#endif
