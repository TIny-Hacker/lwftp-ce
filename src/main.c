#include "defines.h"
#include "ftp.h"
#include "menu.h"
#include "utility.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <graphx.h>
#include <keypadc.h>
#include "lwftp.h"

#include <lwip/core.h>
#include <lwip/conn.h>

struct app_t app = {
    .dirty = ALL_DIRTY,
    .connected = false,
    .ftpResult = LWFTP_RESULT_INPROGRESS,
    .remoteColumn = false,
    .start = {0, 0},
    .selected = {0, 0},
    .total = {0, 0},
    .path = "",
    .dir = 0,
    .busy = false,
};

struct preferences_t prefs = {
    .bgColor = 255,
    .fgColor = 148,
    .hlColor = 222,
    .textColor = 0,
};

static bool lwipStarted = false;

static void main_Delay(unsigned int ms) {
    // Simple busy loop delay (approximate, based on ~15 MHz CPU)
    for (unsigned int i = 0; i < ms; i++) {
        for (volatile unsigned int j = 0; j < 1500; j++) {
            // Busy wait
        }
    }
}

static bool main_StartLwIP(void) {
    if (!lwip_init_runtime()) {
        switch (lwip_runtime_last_error()) {
            case 1:
                menu_PrintMessage("lwIP app missing");
                break;
            case 2:
                menu_PrintMessage("lwIP runtime table");
                break;
            case 3:
                menu_PrintMessage("lwIP runtime count");
                break;
            default:
                menu_PrintMessage("lwIP runtime init");
                break;
        }
        return false;
    }

    if (!lwip_start()) {
        switch (lwip_start_last_error()) {
            case 1:
                menu_PrintMessage("lwIP start init");
                break;
            case 2:
                menu_PrintMessage("lwIP start usb");
                break;
            default:
                menu_PrintMessage("lwIP start failed");
                break;
        }
        return false;
    }

    lwipStarted = true;
    return true;
}

int main(void) {
    static lwftp_session_t s;
    static char user[INPUT_BUF_SIZE];
    static char pass[INPUT_BUF_SIZE];
    static uint8_t server[4] = {0, 0, 0, 0};

    bool keyPressed = false;
    clock_t clockOffset = clock();

    util_ReadConfig();
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetTextFGColor(prefs.textColor);
    util_GetLocalFiles();
    while (kb_AnyKey()); // Debounce

    if (menu_ServerConfig(server, user, pass)) goto exit;
    while (kb_AnyKey());

    gfx_FillScreen(prefs.bgColor);
    if (!main_StartLwIP()) {
        while (!kb_AnyKey());
        goto exit;
    }

    menu_PrintMessage("Waiting for network...");

    app.connected = true;

    memset(&s, 0, sizeof(s));
    IP_ADDR4(&s.server_ip, server[0], server[1], server[2], server[3]);
    s.server_port = 21;
    s.user = user;
    s.pass = pass;
    s.handle = &s;
    s.remote_path = app.path;
    s.done_fn = ftp_ConnectCallback;

    menu_PrintMessage("Connecting to FTP...");

    if (lwftp_connect(&s) != LWFTP_RESULT_INPROGRESS) {
        menu_PrintMessage("Connection failed");
        goto exit;
    }

    while (!app.ftpStarted && app.ftpResult == LWFTP_RESULT_INPROGRESS) {
        lwip_poll_network_events();
    }

    if (!app.ftpStarted) {
        goto exit;
    }

    while (!kb_IsDown(kb_KeyClear)) {
        lwip_poll_network_events();
        kb_Scan();

        if (app.dirty) {
            menu_UpdateMain(&s);
        }

        if (!kb_AnyKey() && keyPressed) {
            keyPressed = false;
            clockOffset = clock();
        }

        if ((kb_Data[7] || kb_Data[1]) && (!keyPressed || clock() - clockOffset > CLOCKS_PER_SEC / 16)) {
            if (kb_IsDown(kb_KeyLeft) || kb_IsDown(kb_KeyRight)) {
                app.remoteColumn = !app.remoteColumn;
                app.dirty |= LOCAL_DIRTY | REMOTE_DIRTY;
            } else if (kb_IsDown(kb_KeyUp)) {
                if (app.selected[app.remoteColumn]) {
                    app.selected[app.remoteColumn] -= 1;

                    if (app.selected[app.remoteColumn] < app.start[app.remoteColumn]) {
                        app.start[app.remoteColumn] -= 1;
                    }
                } else {
                    app.selected[app.remoteColumn] = app.total[app.remoteColumn] - 1;

                    if (app.total[app.remoteColumn] > MAX_SHOWN_FILES) {
                        app.start[app.remoteColumn] = app.total[app.remoteColumn] - MAX_SHOWN_FILES;
                    }
                }

                app.dirty |= app.remoteColumn ? REMOTE_DIRTY : LOCAL_DIRTY;
            } else if (kb_IsDown(kb_KeyDown)) {
                if (app.selected[app.remoteColumn] != app.total[app.remoteColumn] - 1) {
                    app.selected[app.remoteColumn] += 1;

                    if (app.selected[app.remoteColumn] > app.start[app.remoteColumn] + MAX_SHOWN_FILES - 1) {
                        app.start[app.remoteColumn] += 1;
                    }
                } else {
                    app.selected[app.remoteColumn] = 0;
                    app.start[app.remoteColumn] = 0;
                }

                app.dirty |= app.remoteColumn ? REMOTE_DIRTY : LOCAL_DIRTY;
            }

            if (app.ftpStarted && !app.busy) {
                if (kb_IsDown(kb_Key2nd) && REMOTE_FILES[app.selected[app.remoteColumn]].type == TYPE_DIR && RX_BUF_SIZE - strlen(app.path) > 8) {
                    char *insert = app.path + strlen(app.path);
                    *insert = '/';
                    memcpy(insert + 1, REMOTE_FILES[app.selected[app.remoteColumn]].name, 9);
                    util_ChangeDir(&s);
                    app.dir++;
                    while (kb_AnyKey());
                } else if (kb_IsDown(kb_KeyYequ) && app.dir) {
                    *strrchr(app.path, '/') = '\0';
                    util_ChangeDir(&s);
                    app.dir--;
                    while (kb_AnyKey());
                } else if (kb_IsDown(kb_KeyWindow)) {
                    util_UploadFile(&s);
                } else if (kb_IsDown(kb_KeyZoom)) {
                    util_DeleteFile(&s);
                    while (kb_AnyKey());
                } else if (kb_IsDown(kb_KeyTrace)) {
                    util_DownloadFile(&s);
                } else if (kb_IsDown(kb_KeyGraph)) {
                    util_MoveFile(&s);
                    while (kb_AnyKey());
                }
            }

            menu_UpdateMain(&s);
            util_WaitBeforeKeypress(&clockOffset, &keyPressed);
        }
    }

    lwftp_close(&s);
    clockOffset = clock();

    while (s.control_state != LWFTP_CLOSED && clock() - clockOffset < CLOCKS_PER_SEC) {
        lwip_poll_network_events();
        main_Delay(2);
    }

exit:
    gfx_End();
    util_WriteConfig();
    if (lwipStarted) {
        lwip_stop();
    }

    return app.ftpResult;
}
