#include "defines.h"
#include "ftp.h"
#include "menu.h"
#include "utility.h"

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
    util_SortVAT();

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
    memset(app.rxBuf, 0, sizeof(app.rxBuf));
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

void util_DeleteFile(lwftp_session_t *s) {
    if (!app.remoteColumn) {
        uint8_t type = LOCAL_FILES[app.selected[0]].type == TYPE_APPVAR ? OS_TYPE_APPVAR : OS_TYPE_PRGM;
        ti_DeleteVar(LOCAL_FILES[app.selected[0]].name, type);
        util_GetLocalFiles();
    } else {
        if (REMOTE_FILES[app.selected[1]].type == TYPE_DIR) {
            return;
        }

        app.busy = true;

        char *insert = app.path + strlen(app.path);
        *insert = '/';
        memcpy(insert + 1, REMOTE_FILES[app.selected[1]].name, 9);
        insert = app.path + strlen(app.path);
        strcpy(insert, ".8x");
        *(insert + 3) = (REMOTE_FILES[app.selected[1]].type == TYPE_APPVAR) ? 'v' : 'p';
        *(insert + 4) = '\0';
        s->remote_path = app.path;
        s->done_fn = ftp_DelCallback;

        lwftp_delete(s);
    }

    app.total[app.remoteColumn]--;

    if (app.total[app.remoteColumn] && app.selected[app.remoteColumn] == app.total[app.remoteColumn]) {
        app.selected[app.remoteColumn]--;
    }
}

void util_MoveFile(lwftp_session_t *s) {
    if (!app.remoteColumn || REMOTE_FILES[app.selected[1]].type == TYPE_DIR) {
        return;
    }

    char *insert = app.path + strlen(app.path);
    *insert = '/';
    memcpy(insert + 1, REMOTE_FILES[app.selected[1]].name, 9);
    insert = app.path + strlen(app.path);
    strcpy(insert, ".8x");
    *(insert + 3) = (REMOTE_FILES[app.selected[1]].type == TYPE_APPVAR) ? 'v' : 'p';
    *(insert + 4) = '\0';
    s->remote_path = app.path;
    static char input[INPUT_BUF_SIZE * 2];
    memset(input, 0, INPUT_BUF_SIZE * 2);
    menu_StringInput(6, 6, 308, input);

    if (*input == '\0') {
        return;
    }

    insert = input + strlen(input);

    if (*(insert - 1) == '/') {
        memcpy(insert, REMOTE_FILES[app.selected[1]].name, 9);
        insert = input + strlen(input);
        strcpy(insert, ".8x");
        *(insert + 3) = (REMOTE_FILES[app.selected[1]].type == TYPE_APPVAR) ? 'v' : 'p';
        *(insert + 4) = '\0';
    }

    s->remote_new_path = input;
    s->done_fn = ftp_MoveCallback;

    app.busy = true;
    lwftp_move(s);
}

void util_UploadFile(lwftp_session_t *s) {
    if (app.remoteColumn) {
        return;
    }

    menu_PrintMessage("Uploading file...");

    char *insert = app.path + strlen(app.path);
    *insert = '/';
    memcpy(insert + 1, LOCAL_FILES[app.selected[0]].name, 9);
    insert = app.path + strlen(app.path);
    strcpy(insert, ".8x");
    *(insert + 3) = (LOCAL_FILES[app.selected[0]].type == TYPE_APPVAR) ? 'v' : 'p';
    *(insert + 4) = '\0';
    s->remote_path = app.path;

    const char header[] = {'*', '*', 'T', 'I', '8', '3', 'F', '*', 0x1A, 0x0A, 0x00};
    const char *comment = "Uploaded by lwFTP CE";
    uint8_t *buf = FILE_BUFFER;

    memset(buf, 0, 65535);
    memcpy(buf, header, 11);
    buf += 11;
    memcpy(buf, comment, 21);
    buf += 42;
    uint8_t type = LOCAL_FILES[app.selected[0]].type == TYPE_APPVAR ? OS_TYPE_APPVAR : OS_TYPE_PRGM;
    uint8_t slot = ti_OpenVar(LOCAL_FILES[app.selected[0]].name, "r", type);
    type = *(uint8_t *)ti_GetVATPtr(slot);
    uint16_t size = ti_GetSize(slot);
    *(uint16_t *)buf = size + 19;
    buf += 2;
    *(uint16_t *)buf = 0xD;
    buf += 2;
    *(uint16_t *)buf = size + 2;
    buf += 2;
    *(uint8_t *)(buf++) = type;
    memcpy(buf, LOCAL_FILES[app.selected[0]].name, 8);
    buf += 9; // Name + version byte
    if (ti_IsArchived(slot)) {
        *(uint8_t *)(buf++) = 0x80;
    }
    *(uint16_t *)buf = size + 2;
    buf += 2;
    memcpy(buf, ti_GetDataPtr(slot) - 2, 2 + size); // variable entry + data
    ti_Close(slot);
    *(uint16_t *)(buf + 2 + size) = util_ComputeChecksum(FILE_BUFFER + 11 + 42 + 2, 17 + size);

    app.txSize = 11 + 42 + 2 + 17 + size + 4; // Header + comment + size + variable entry + data + checksum
    app.txOffset = 0;
    app.busy = true;

    s->data_source = ftp_StorDataSource;
    s->done_fn = ftp_StorCallback;

    lwftp_store(s);

    while (app.busy && !kb_IsDown(kb_KeyClear)) {
        kb_Scan();
        util_ServiceNetwork();
    }

    app.dirty = ALL_DIRTY;
}

void util_DownloadFile(lwftp_session_t *s) {
    if (!app.remoteColumn) {
        return;
    }

    menu_PrintMessage("Downloading file...");

    char *insert = app.path + strlen(app.path);
    *insert = '/';
    memcpy(insert + 1, REMOTE_FILES[app.selected[1]].name, 9);
    insert = app.path + strlen(app.path);
    strcpy(insert, ".8x");
    *(insert + 3) = (REMOTE_FILES[app.selected[1]].type == TYPE_APPVAR) ? 'v' : 'p';
    *(insert + 4) = '\0';
    s->remote_path = app.path;

    memset(FILE_BUFFER, 0, 65535);
    app.rxOffset = 0;
    app.busy = true;

    s->data_sink = ftp_RetrDataSink;
    s->done_fn = ftp_RetrCallback;

    lwftp_retrieve(s);

    while (app.busy && !kb_IsDown(kb_KeyClear)) {
        kb_Scan();
        util_ServiceNetwork();
    }

    *strrchr(app.path, '/') = '\0';
    app.dirty = ALL_DIRTY;
    s->done_fn = NULL;
}

uint16_t util_ComputeChecksum(uint8_t *data, unsigned int size) {
    uint16_t sum = 0;

    for (unsigned int i = 0; i < size; i++) {
        sum += data[i];
    }

    return sum;
}
