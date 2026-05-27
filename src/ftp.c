#include "defines.h"
#include "ftp.h"
#include "lwftp.h"
#include "menu.h"
#include "utility.h"

#include <string.h>

void ftp_ConnectCallback(void *arg, int result) {
    lwftp_session_t *s = (lwftp_session_t *)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_LOGGED) {
        app.ftpResult = result;
        lwftp_close(s);
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
        app.rxBuf[app.rxOffset] = *(ptr++);
        len--;

        if (*ptr == '\n') {
            app.rxBuf[app.rxOffset] = '\0'; // Overwrites \r
            char *name = strchr(app.rxBuf, ' ') + 1;
            char *type = strstr(app.rxBuf, "type") + 5;

            if (*type == 'd') {
                REMOTE_FILES[app.total[1]].type = TYPE_DIR;
            } else {
                uint8_t len = strlen(app.rxBuf);
                if (!strncmp(&(app.rxBuf[len - 4]), ".8x", 3)) {
                    if (app.rxBuf[len - 1] == 'p') {
                        REMOTE_FILES[app.total[1]].type = TYPE_PROG;
                    } else if (app.rxBuf[len - 1] == 'v') {
                        REMOTE_FILES[app.total[1]].type = TYPE_APPVAR;
                    }
                }
            }

            if (REMOTE_FILES[app.total[1]].type != TYPE_UNKNOWN) {
                for (uint8_t i = 0; i < 8 && name[i] != '\0'; i++) {
                    if (*type != 'd' && name[i] == '.') break;
                    REMOTE_FILES[app.total[1]].name[i] = name[i];
                }

                app.total[1]++;
            }

            app.rxOffset = 0;
            ptr++; // Skip newline
            len--;
        } else if (app.rxOffset != RX_BUF_SIZE) {
            app.rxOffset++;
        } else {
            break;
        }
    }

    return len;
}

void ftp_ListCallback(void *arg, int result) {
    (void)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        app.ftpResult = result;
        return;
    }

    app.dirty |= REMOTE_DIRTY;
}
