#include "defines.h"
#include "ftp.h"
#include "lwftp.h"
#include "menu.h"
#include "utility.h"

#include <fileioc.h>
#include <keypadc.h>
#include <string.h>

static void ftp_AddRemoteFile(void) {
    char *name = strchr(app.rxBuf, ' ');
    char *type = strstr(app.rxBuf, "type=");

    if (!name || !type || app.total[1] >= MAX_REMOTE_FILES) {
        return;
    }

    name++;
    type += 5;

    REMOTE_FILES[app.total[1]].type = TYPE_UNKNOWN;

    if (!strncmp(type, "dir", 3)) {
        REMOTE_FILES[app.total[1]].type = TYPE_DIR;
    } else {
        size_t name_len = strlen(name);

        if (name_len >= 4 && !strncmp(name + name_len - 4, ".8x", 3)) {
            if (name[name_len - 1] == 'p') {
                REMOTE_FILES[app.total[1]].type = TYPE_PROG;
            } else if (name[name_len - 1] == 'v') {
                REMOTE_FILES[app.total[1]].type = TYPE_APPVAR;
            }
        }
    }

    if (REMOTE_FILES[app.total[1]].type != TYPE_UNKNOWN) {
        for (uint8_t i = 0; i < 8 && name[i] != '\0'; i++) {
            if (REMOTE_FILES[app.total[1]].type != TYPE_DIR && name[i] == '.') break;
            REMOTE_FILES[app.total[1]].name[i] = name[i];
        }

        app.total[1]++;
    }
}

void ftp_ConnectCallback(void *arg, int result) {
    lwftp_session_t *s = (lwftp_session_t *)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_LOGGED) {
        app.ftpResult = result;
        lwftp_close(s);
        app.busy = false;
        return;
    }

    menu_PrintMessage("Logged in");
    app.ftpStarted = true;
    util_GetRemoteFiles(s);
}

uint16_t ftp_ListDataSink(void *arg, const char *ptr, uint16_t len) {
    (void)arg;

    if (ptr == NULL) {
        return 0;
    }

    while (len) {
        char c = *(ptr++);
        len--;

        if (c == '\n') {
            if (app.rxOffset && app.rxBuf[app.rxOffset - 1] == '\r') {
                app.rxOffset--;
            }

            app.rxBuf[app.rxOffset] = '\0';
            ftp_AddRemoteFile();
            app.rxOffset = 0;
        } else if (app.rxOffset < RX_BUF_SIZE) {
            app.rxBuf[app.rxOffset++] = c;
        } else {
            break;
        }
    }

    app.dirty |= REMOTE_DIRTY;
    return len;
}

void ftp_ListCallback(void *arg, int result) {
    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    util_GetDir((lwftp_session_t *)arg);
}

uint16_t ftp_PwdDataSink(void *arg, const char *ptr, uint16_t len) {
    (void)arg;

    if (ptr == NULL) {
        return 0;
    }

    const char *start = memchr(ptr, '"', len);
    if (start) {
        start++;
        const char *end = memchr(start, '"', len - (start - ptr));
        if (end) {
            unsigned int copylen = end - start > RX_BUF_SIZE ? RX_BUF_SIZE : end - start;
            memcpy(app.path, start, copylen);
            app.path[copylen] = '\0';
            app.rxOffset = copylen;
        }
    }

    app.dirty |= PATH_DIRTY;
    return len;
}

void ftp_PwdCallback(void *arg, int result) {
    (void)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    app.busy = false;
}

void ftp_CwdCallback(void *arg, int result) {
    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    app.dirty |= PATH_DIRTY;
    util_GetRemoteFiles((lwftp_session_t *)arg);
}

void ftp_DelCallback(void *arg, int result) {
    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    *strrchr(app.path, '/') = '\0';
    util_GetRemoteFiles((lwftp_session_t *)arg);
}

void ftp_MoveCallback(void *arg, int result) {
    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    *strrchr(app.path, '/') = '\0';

    app.total[app.remoteColumn]--;

    if (app.total[app.remoteColumn] && app.selected[app.remoteColumn] == app.total[app.remoteColumn]) {
        app.selected[app.remoteColumn]--;
    }

    util_GetRemoteFiles((lwftp_session_t *)arg);
}

uint16_t ftp_StorDataSource(void *arg, const char **pptr, uint16_t maxlen) {
    (void)arg;

    if (pptr == NULL) {
        app.txOffset += maxlen;
    }

    if (app.txOffset > app.txSize) {
        app.txOffset = app.txSize;
    }

    if (pptr == NULL || maxlen == 0 || app.txOffset == app.txSize) {
        return 0;
    }

    unsigned int len = app.txSize - app.txOffset;

    if (len > maxlen) {
        len = maxlen;
    }

    *pptr = (const char *)(FILE_BUFFER + app.txOffset);
    return (uint16_t)len;
}

void ftp_StorCallback(void *arg, int result) {
    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    *strrchr(app.path, '/') = '\0';

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        app.busy = false;
        return;
    }

    util_GetRemoteFiles((lwftp_session_t *)arg);
}

uint16_t ftp_RetrDataSink(void *arg, const char *ptr, uint16_t len) {
    (void)arg;

    if (ptr == NULL) {
        return 0;
    }

    if (app.rxOffset + len > 65535) {
        len = 65535 - app.rxOffset;
    }

    memcpy(&(FILE_BUFFER[app.rxOffset]), ptr, len);
    app.rxOffset += len;

    return len;
}

void ftp_RetrCallback(void *arg, int result) {
    (void)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    app.busy = false;

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        return;
    }

    // char name[9];
    // memset(name, 0, 9);
    // uint8_t *buf = FILE_BUFFER;
    // buf += 53;
    // uint16_t dataSize = *(uint16_t *)buf;
    // buf += 2;
    // uint16_t checksum = util_ComputeChecksum(buf, dataSize);
    // buf += 4;
    // uint8_t type = *(buf++);
    // memcpy(name, buf, 8);
    // buf += 9;
    // bool archived = (*(buf++) == 0x80) ? true : false;
    // uint16_t size = *(uint16_t *)buf;
    // buf += 2;

    // if (checksum != *(uint16_t *)(buf + size)) {
    //     menu_PrintMessage("Invalid checksum");
    //     while (!kb_AnyKey()) {
    //         util_ServiceNetwork();
    //     }
    //     return;
    // }

    // uint8_t slot = ti_OpenVar(name, "w", type);
    // ti_Write(buf + 2, sizeof(uint8_t), size - 2, slot);
    // ti_SetArchiveStatus(archived, slot);
    // ti_Close(slot);

    // util_GetLocalFiles();
}
