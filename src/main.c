#include "defines.h"
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

static const char test_buf[] =
    "Sent by TI-84 Plus CE :)\r\n";

struct app_t app = {
    .dirty = ALL_DIRTY,
    .ftp_result = LWFTP_RESULT_INPROGRESS,
    .selectedLocal = 0,
    .selectedRemote = 0,
    .startLocal = 0,
    .startRemote = 0,
    .remoteColumn = false,
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

static void main_ServiceNetwork(void) {
    usb_HandleEvents();
    sys_check_timeouts();
}

static bool main_WaitForIP(void) {
    for (unsigned int tick = 0; tick < IP_WAIT_TICKS; tick++) {
        main_ServiceNetwork();
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

static uint16_t ftp_DataSource(void *arg, const char **pptr, uint16_t maxlen) {
    unsigned int remaining;
    unsigned int len;

    (void)arg;

    if (pptr == NULL) {
        tx_offset += maxlen;

        if (tx_offset > sizeof(test_buf) - 1u) {
            tx_offset = sizeof(test_buf) - 1u;
        }

        return 0;
    }

    remaining = (unsigned int)(sizeof(test_buf) - 1u) - tx_offset;
    len = remaining;

    if (len > maxlen) {
        len = maxlen;
    }

    *pptr = test_buf + tx_offset;
    return (uint16_t)len;
}

static uint16_t ftp_DataSink(void *arg, const char *ptr, uint16_t len) {
    unsigned int remaining;
    unsigned int copy_len;

    (void)arg;

    if (ptr == NULL) {
        return 0;
    }

    remaining = (unsigned int)sizeof(rx_buf) - rx_offset;
    copy_len = len;

    if (copy_len > remaining) {
        copy_len = remaining;
        rx_overflow = true;
    }

    if (copy_len > 0) {
        memcpy(rx_buf + rx_offset, ptr, copy_len);
        rx_offset += copy_len;
    }

    return len;
}

static void ftp_RetrCallback(void *arg, int result) {
    lwftp_session_t *s = (lwftp_session_t *)arg;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result == LWFTP_RESULT_OK) {
        if (rx_overflow || rx_offset != sizeof(test_buf) - 1u || strcmp(rx_buf, test_buf) != 0) {
            menu_PrintMessage("RETR test failed");
            result = LWFTP_RESULT_ERR_LOCAL;
        } else {
            menu_PrintMessage("RETR test success");
        }
    }

    ftp_result = result;
    ftp_finished = true;
    s->done_fn = NULL;
    lwftp_close(s);
}

static void ftp_StorCallback(void *arg, int result) {
    lwftp_session_t *s = (lwftp_session_t *)arg;
    err_t err;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_OK) {
        ftp_result = result;
        ftp_finished = true;
        lwftp_close(s);
        return;
    }

    menu_PrintMessage("STOR test success");
    memset(rx_buf, 0, sizeof(rx_buf));
    rx_offset = 0;
    rx_overflow = false;

    s->data_source = NULL;
    s->data_sink = ftp_DataSink;
    s->done_fn = ftp_RetrCallback;
    s->remote_path = FTP_REMOTE_PATH;

    err = lwftp_retrieve(s);

    if (err != LWFTP_RESULT_INPROGRESS) {
        ftp_result = err;
        ftp_finished = true;
    }
}

static void ftp_ConnectCallback(void *arg, int result) {
    lwftp_session_t *s = (lwftp_session_t *)arg;
    err_t err;

    if (result == LWFTP_RESULT_INPROGRESS) {
        return;
    }

    if (result != LWFTP_RESULT_LOGGED) {
        ftp_result = result;
        ftp_finished = true;
        lwftp_close(s);
        return;
    }

    menu_PrintMessage("Logged in");

    s->data_sink = NULL;
    s->data_source = ftp_DataSource;
    s->done_fn = ftp_StorCallback;
    s->remote_path = FTP_REMOTE_PATH;

    err = lwftp_store(s);
    if (err != LWFTP_RESULT_INPROGRESS) {
        ftp_result = err;
        ftp_finished = true;
    }
}

int main(void) {
    static lwftp_session_t s;
    static char user[MAX_INPUT_LENGTH];
    static char pass[MAX_INPUT_LENGTH];
    static uint8_t server[4] = {0, 0, 0, 0};

    util_ReadConfig();
    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetTextFGColor(prefs.textColor);
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

    memset(&s, 0, sizeof(s));
    IP_ADDR4(&s.server_ip, server[0], server[1], server[2], server[3]);
    s.server_port = 21;
    s.user = user;
    s.pass = pass;
    s.handle = &s;
    s.done_fn = ftp_ConnectCallback;

    menu_PrintMessage("Connecting to FTP...");

    if (lwftp_connect(&s) != LWFTP_RESULT_INPROGRESS) {
        menu_PrintMessage("Connection failed");
        goto exit;
    }

    while (!ftp_finished) {
        main_ServiceNetwork();
        menu_UpdateMain(&s);
        kb_Scan();

        if (kb_IsDown(kb_KeyClear)) {
            ftp_result = LWFTP_RESULT_ERR_LOCAL;
            ftp_finished = true;
            lwftp_close(&s);
            break;
        }

        delay(2);
    }

    while (s.control_state != LWFTP_CLOSED) {
        main_ServiceNetwork();
        delay(2);
    }

    if (ftp_started) lwftp_close(&s);

exit:
    while (!kb_AnyKey());
    gfx_End();
    util_WriteConfig();
    usb_Cleanup();

    return ftp_result;
}
