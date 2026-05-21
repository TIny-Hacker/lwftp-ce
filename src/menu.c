#include "defines.h"
#include "utility.h"

#include <graphx.h>
#include <keypadc.h>
#include <string.h>

/**
 * @brief Draw a filled rectangle with one pixel indentations in the corners.
 * 
 * @param x Top-left x coordinate.
 * @param y Top-left y coordinate.
 * @param width Rectangle width.
 * @param height Rectangle height.
 */
static void menu_PixelIndentRectangle(unsigned int x, uint8_t y, unsigned int width, uint8_t height) {
    gfx_FillRectangle(x + 1, y, width - 2, height);
    gfx_Rectangle(x, y + 1, width, height - 2);
}

/**
 * @brief Print out an address formatted X.X.X.X
 * 
 * @param addr 4-byte array to print.
 * @param x Top-left x coordinate.
 * @param y Top-left y coordinate.
 */
static void menu_PrintAddress(uint8_t *addr, unsigned int x, uint8_t y) {
    gfx_SetTextXY(x, y);
    gfx_PrintUInt(addr[0], 1);
    gfx_PrintChar('.');
    gfx_PrintUInt(addr[1], 1);
    gfx_PrintChar('.');
    gfx_PrintUInt(addr[2], 1);
    gfx_PrintChar('.');
    gfx_PrintUInt(addr[3], 1);
}

/**
 * @brief Very simple text input routine. Could probably be improved later.
 * 
 * @param x Top-left x coordinate.
 * @param y Top-left y coordinate.
 * @param stringLength Maximum input width in pixels.
 * @param input Input buffer.
 * @return int8_t 0 if successful.
 */
static int8_t menu_StringInput(unsigned int x, uint8_t y, unsigned int width, char *input) {
    bool keyPressed = false;
    uint8_t currentOffset = 0;
    uint8_t inputMode = INPUT_DEFAULT;
    char c = '\0';

    clock_t clockOffset = clock();

    memset(input, '\0', MAX_INPUT_LENGTH);

    gfx_SetColor(prefs.hlColor);
    gfx_FillRectangle_NoClip(x, y, 108, 8);

    while (!kb_IsDown(kb_KeyClear) && !(kb_IsDown(kb_Key2nd) && currentOffset > 0)) {
        kb_Scan();

        if (!kb_AnyKey() && keyPressed) {
            keyPressed = false;
            clockOffset = clock();
        }

        if (kb_AnyKey() && (!keyPressed || clock() - clockOffset > CLOCKS_PER_SEC / 16)) {
            clockOffset = clock();

            if (kb_IsDown(kb_KeyClear) || (kb_IsDown(kb_Key2nd) && currentOffset > 0)) {
                break;
            } else if (kb_IsDown(kb_KeyAlpha)) {
                inputMode = inputMode == INPUT_LOWER ? INPUT_DEFAULT : inputMode + 1;
                while (kb_AnyKey());
            } else if (currentOffset < MAX_INPUT_LENGTH - 1) {
                if (!keyPressed) {
                    c = asm_util_GetCharFromKey(inputMode);
                }

                if (c >= ' ' && c <= '~') {
                    input[currentOffset] = c;

                    if (gfx_GetStringWidth(input) > width) {
                        input[currentOffset] = '\0';
                    } else {
                        currentOffset++;
                    }
                }
            }

            gfx_PrintStringXY(input, x, y);
            gfx_BlitBuffer();

            util_WaitBeforeKeypress(&clockOffset, &keyPressed);
        }
    }

    if (kb_IsDown(kb_Key2nd)) {
        return 0;
    }

    if (kb_IsDown(kb_KeyClear)) {
        while (kb_AnyKey());
    }

    return 1;
}

void menu_PrintMessage(char *message) {
    gfx_BlitBuffer();
    gfx_SetDrawScreen();
    gfx_SetColor(prefs.hlColor);
    unsigned int mWidth = gfx_GetStringWidth(message);
    menu_PixelIndentRectangle(160 - mWidth / 2 - 6, 110, mWidth + 6 * 2, 19);
    gfx_SetColor(prefs.bgColor);
    menu_PixelIndentRectangle(160 - mWidth / 2 - 4, 112, mWidth + 8, 15);
    gfx_PrintStringXY(message, 160 - mWidth / 2, 116);
    gfx_SetDrawBuffer();
    while (kb_AnyKey());
}

int8_t menu_ClientConfig(void) {
    uint8_t option = 0;
    bool redraw = true;
    bool keyPressed = false;
    clock_t clockOffset = clock();

    gfx_FillScreen(prefs.bgColor);
    gfx_PrintStringXY("Client Setup", 119, 86);
    gfx_SetColor(prefs.fgColor);
    menu_PixelIndentRectangle(65, 101, 191, 53);
    while (kb_AnyKey());

    while (!kb_IsDown(kb_KeyClear) && !kb_IsDown(kb_KeyEnter)) {
        kb_Scan();

        if (!kb_AnyKey() && keyPressed) {
            keyPressed = false;
            clockOffset = clock();
        }

        if ((kb_Data[7] || kb_IsDown(kb_Key2nd)) && (!keyPressed || clock() - clockOffset > CLOCKS_PER_SEC / 12)) {
            redraw = true;

            if (kb_IsDown(kb_KeyUp) || kb_IsDown(kb_KeyLeft)) {
                option = option ? option - 1 : 2;
            } else if (kb_IsDown(kb_KeyDown) || kb_IsDown(kb_KeyRight)) {
                option = option < 2 ? option + 1 : 0;
            } else if (kb_IsDown(kb_Key2nd)) {
                while (kb_AnyKey());
                char input[MAX_INPUT_LENGTH];
                uint8_t addr[4];

                if (menu_StringInput(142, 111 + option * 13, 106, input)) {
                    while (kb_AnyKey());
                    continue;
                }

                if (!util_ParseAddr(input, addr)) {
                    switch (option) {
                    case 0:
                        memcpy(prefs.client, addr, 4);
                        break;
                    case 1:
                        memcpy(prefs.mask, addr, 4);
                        break;
                    case 2:
                        memcpy(prefs.gw, addr, 4);
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        if (redraw) {
            redraw = false;
            gfx_SetColor(prefs.bgColor);
            menu_PixelIndentRectangle(67, 103, 187, 49);
            gfx_SetColor(prefs.hlColor);
            menu_PixelIndentRectangle(71, 107 + option * 13, 179, 15);
            gfx_PrintStringXY("IP:", 74, 111);
            gfx_PrintStringXY("Mask:", 75, 124);
            gfx_PrintStringXY("Gateway:", 75, 137);
            menu_PrintAddress(prefs.client, 142, 111);
            menu_PrintAddress(prefs.mask, 142, 124);
            menu_PrintAddress(prefs.gw, 142, 137);
            gfx_BlitBuffer();
            util_WaitBeforeKeypress(&clockOffset, &keyPressed);
        }
    }

    return kb_IsDown(kb_KeyClear);
}

int8_t menu_ServerConfig(uint8_t *server, char *user, char *pass) {
    uint8_t option = 0;
    bool redraw = true;
    bool keyPressed = false;
    clock_t clockOffset = clock();

    gfx_FillScreen(prefs.bgColor);
    gfx_PrintStringXY("Server Setup", 115, 86);
    gfx_SetColor(prefs.fgColor);
    menu_PixelIndentRectangle(65, 101, 191, 53);
    while (kb_AnyKey());

    while (!kb_IsDown(kb_KeyClear) && !kb_IsDown(kb_KeyEnter)) {
        kb_Scan();

        if (!kb_AnyKey() && keyPressed) {
            keyPressed = false;
            clockOffset = clock();
        }

        if ((kb_Data[7] || kb_IsDown(kb_Key2nd)) && (!keyPressed || clock() - clockOffset > CLOCKS_PER_SEC / 12)) {
            redraw = true;

            if (kb_IsDown(kb_KeyUp) || kb_IsDown(kb_KeyLeft)) {
                option = option ? option - 1 : 2;
            } else if (kb_IsDown(kb_KeyDown) || kb_IsDown(kb_KeyRight)) {
                option = option < 2 ? option + 1 : 0;
            } else if (kb_IsDown(kb_Key2nd)) {
                while (kb_AnyKey());
                char input[MAX_INPUT_LENGTH];
                uint8_t addr[4];

                if (menu_StringInput(142, 111 + option * 13, 106, input)) {
                    while (kb_AnyKey());
                    continue;
                }

                switch (option) {
                case 0:
                    if (!util_ParseAddr(input, addr)) {
                        memcpy(server, addr, 4);
                    }
                    break;
                case 1:
                    strcpy(user, input);
                    break;
                case 2:
                    strcpy(pass, input);
                    break;
                default:
                    break;
                }
            }
        }

        if (redraw) {
            redraw = false;
            gfx_SetColor(prefs.bgColor);
            menu_PixelIndentRectangle(67, 103, 187, 49);
            gfx_SetColor(prefs.hlColor);
            menu_PixelIndentRectangle(71, 107 + option * 13, 179, 15);
            gfx_PrintStringXY("IP:", 74, 111);
            gfx_PrintStringXY("User:", 75, 124);
            gfx_PrintStringXY("Pass:", 75, 137);
            menu_PrintAddress(server, 142, 111);
            gfx_PrintStringXY(user, 142, 124);
            gfx_PrintStringXY(pass, 142, 137);
            gfx_BlitBuffer();
            util_WaitBeforeKeypress(&clockOffset, &keyPressed);
        }
    }

    return kb_IsDown(kb_KeyClear);
}


