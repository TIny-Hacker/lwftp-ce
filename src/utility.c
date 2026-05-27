#include "defines.h"
#include "ftp.h"

#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usbdrvce.h>

#include "lwip/timeouts.h"

void util_ServiceNetwork(void) {
    usb_HandleEvents();
    sys_check_timeouts();
}

void util_WaitBeforeKeypress(clock_t *clockOffset, bool *keyPressed) {
    if (!(*keyPressed)) {
        while ((clock() - *clockOffset < CLOCKS_PER_SEC / 2.25) && kb_AnyKey()) {
            if (app.connected) {
                util_ServiceNetwork();
            }

            kb_Scan();
        }
    }

    *keyPressed = true;
    *clockOffset = clock();
}

int8_t util_ParseAddr(char *s, uint8_t *addr) {
    for (uint8_t i = 0; i < 4; i++) {
        addr[i] = strtoul(s, &s, 10);
        if (*s == '.') {
            s++;
        } else if (!(*s == '\0' && i == 3)) {
            return 1;
        }
    }

    return 0;
}

void util_ReadConfig(void) {
    uint8_t slot = ti_Open("lwFTP", "r");

    if (slot) {
        ti_Read(&prefs, sizeof(prefs), 1, slot);
    }

    ti_Close(slot);
    slot = ti_Open("CEaShell", "r");

    if (slot) { // Sync theme with CEaShell if installed
        ti_Seek(sizeof(uint8_t) * 2, SEEK_SET, slot);
        ti_Read(&prefs, sizeof(uint8_t), 4, slot);
    }

    uint8_t transparentColor = 248;

    while (transparentColor == prefs.textColor) {
        transparentColor++;
    }

    gfx_SetTextBGColor(transparentColor);
    gfx_SetTextTransparentColor(transparentColor);
}

void util_WriteConfig(void) {
    uint8_t slot = ti_Open("lwFTP", "w+");
    ti_Write(&prefs, sizeof(prefs), 1, slot);
    ti_SetArchiveStatus(true, slot);
    ti_Close(slot);
}

void util_GetLocalFiles(void) {
    uint8_t type;
    char *name;
    void *vat = NULL;
    unsigned int i = 0;
    memset(LOCAL_FILES, 0, sizeof(struct file_t) * MAX_LOCAL_FILES);

    while ((name = ti_DetectAny(&vat, NULL, &type))) {
        if (*name == '!' || *name == '#') {
            continue;
        }

        if (type == OS_TYPE_APPVAR) {
            LOCAL_FILES[i].type = TYPE_APPVAR;
        } else if (type == OS_TYPE_PROT_PRGM || type == OS_TYPE_PRGM) {
            LOCAL_FILES[i].type = TYPE_PROG;
        } else {
            continue;
        }

        memcpy(LOCAL_FILES[i].name, name, sizeof(char) * 8);
        if (LOCAL_FILES[i].name[0] < 'A') LOCAL_FILES[i].name[0] += 64; // Account for hidden files
        i++;
    }

    app.total[0] = i;
    app.dirty |= LOCAL_DIRTY;
}

void util_GetRemoteFiles(lwftp_session_t *s) {
    app.busy = true;

    s->data_sink = ftp_ListDataSink;
    s->done_fn = ftp_ListCallback;

    app.rxOffset = 0;
    app.total[1] = 0;
    memset(REMOTE_FILES, 0, sizeof(struct file_t) * MAX_REMOTE_FILES);

    lwftp_mlsd(s);
}

void util_GetDir(lwftp_session_t *s) {
    app.busy = true;

    s->data_sink = ftp_PwdDataSink;
    s->done_fn = ftp_PwdCallback;

    app.rxOffset = 0;
    memset(app.path, 0, RX_BUF_SIZE + 2);

    lwftp_print_dir(s);
}

void util_ChangeDir(lwftp_session_t *s) {
    app.busy = true;

    app.selected[1] = 0;
    app.start[1] = 0;
    s->remote_path = app.path;
    s->done_fn = ftp_CwdCallback;

    lwftp_change_dir(s);
}
