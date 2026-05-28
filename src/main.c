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
#include <usbdrvce.h>

#include "drivers/mem.h"
#include "drivers/usb_ethernet.h"
#include "lwftp.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"

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
    .client = {0, 0, 0, 0},
    .mask = {0, 0, 0, 0},
    .gw = {0, 0, 0, 0},
};

static void delay(unsigned int ms) {
    // Simple busy loop delay (approximate, based on ~15 MHz CPU)
    for (unsigned int i = 0; i < ms; i++) {
        for (volatile unsigned int j = 0; j < 1500; j++) {
            // Busy wait
        }
    }
}

static bool main_WaitForIP(void) {
    for (unsigned int tick = 0; tick < IP_WAIT_TICKS; tick++) {
        util_ServiceNetwork();
        kb_Scan();

        if (netif_default) {
            ip4_addr_t ip, gw, mask;

            IP4_ADDR(&ip, prefs.client[0], prefs.client[1], prefs.client[2], prefs.client[3]);
            IP4_ADDR(&mask, prefs.mask[0], prefs.mask[1], prefs.mask[2], prefs.mask[3]);
            IP4_ADDR(&gw, prefs.gw[0], prefs.gw[1], prefs.gw[2], prefs.gw[3]);
            netif_set_addr(netif_default, &ip, &mask, &gw);
            return !ip4_addr_isany(netif_ip4_addr(netif_default));
        }

        if (kb_IsDown(kb_KeyClear)) {
            return false;
        }

        delay(10);
    }

    return false;
}

static bool main_StartLwIP(void) {
    if (!mem_init(LWIP_MAX_HEAP, malloc, free, realloc)) {
        menu_PrintMessage("mem_init failed");
        return false;
    }

    struct lwip_configurator conf = {0};
    conf.version = LWIP_CONFIGURATOR_V1;
    conf.usb_conf.reset_device = usb_ResetDevice;
    conf.usb_conf.disable_device = usb_DisableDevice;
    conf.usb_conf.ref_device = usb_RefDevice;
    conf.usb_conf.unref_device = usb_UnrefDevice;
    conf.usb_conf.set_device_data = usb_SetDeviceData;
    conf.usb_conf.get_device_data = usb_GetDeviceData;
    conf.usb_conf.get_role = usb_GetRole;
    conf.usb_conf.get_device_flags = usb_GetDeviceFlags;
    conf.usb_conf.schedule_transfer = usb_ScheduleTransfer;
    conf.usb_conf.control_transfer = usb_ControlTransfer;
    conf.usb_conf.get_config_descriptor_len = usb_GetConfigurationDescriptorTotalLength;
    conf.usb_conf.get_descriptor = usb_GetDescriptor;
    conf.usb_conf.get_string_descriptor = usb_GetStringDescriptor;
    conf.usb_conf.set_configuration = usb_SetConfiguration;
    conf.usb_conf.set_interface = usb_SetInterface;
    conf.usb_conf.get_device_endpoint = usb_GetDeviceEndpoint;
    conf.usb_conf.set_endpoint_data = usb_SetEndpointData;
    conf.usb_conf.get_endpoint_data = usb_GetEndpointData;
    conf.usb_conf.set_endpoint_flags = usb_SetEndpointFlags;
    conf.malloc_conf.caller_malloc = malloc;
    conf.malloc_conf.caller_free = free;

    if (lwip_init(&conf) != ERR_OK) {
        menu_PrintMessage("lwip_init failed");
        return false;
    }

    if (usb_Init(eth_usb_event_callback, NULL, NULL, USB_DEFAULT_INIT_FLAGS)) {
        menu_PrintMessage("usb_Init failed");
        return false;
    }

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

    if (menu_ClientConfig()) goto exit;
    while (kb_AnyKey());

    if (menu_ServerConfig(server, user, pass)) goto exit;
    while (kb_AnyKey());

    gfx_FillScreen(prefs.bgColor);
    if (!main_StartLwIP()) {
        while (!kb_AnyKey());
        goto exit;
    }

    menu_PrintMessage("Waiting for network...");

    if (!main_WaitForIP()) {
        usb_Cleanup();
        menu_PrintMessage("Bad client IP");
        goto exit;
    }

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
        util_ServiceNetwork();
    }

    if (!app.ftpStarted) {
        goto exit;
    }

    while (!kb_IsDown(kb_KeyClear)) {
        util_ServiceNetwork();
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
                } else if (kb_IsDown(kb_KeyZoom)) {
                    util_DeleteFile(&s);
                    while (kb_AnyKey());
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
        util_ServiceNetwork();
        delay(2);
    }

exit:
    gfx_End();
    util_WriteConfig();
    usb_Cleanup();

    return app.ftpResult;
}
