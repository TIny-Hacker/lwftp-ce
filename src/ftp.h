#ifndef FTP_H
#define FTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ftp_ConnectCallback(void *arg, int result);

uint16_t ftp_ListDataSink(void *arg, const char *ptr, uint16_t len);

void ftp_ListCallback(void *arg, int result);

uint16_t ftp_PwdDataSink(void *arg, const char *ptr, uint16_t len);

void ftp_PwdCallback(void *arg, int result);

void ftp_CwdCallback(void *arg, int result);

void ftp_DelCallback(void *arg, int result);

void ftp_MoveCallback(void *arg, int result);

uint16_t ftp_StorDataSource(void *arg, const char **pptr, uint16_t maxlen);

void ftp_StorCallback(void *arg, int result);

uint16_t ftp_RetrDataSink(void *arg, const char *ptr, uint16_t len);

void ftp_RetrCallback(void *arg, int result);

#ifdef __cplusplus
}
#endif

#endif
