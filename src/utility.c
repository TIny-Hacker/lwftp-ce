#include "defines.h"

#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdlib.h>
#include <time.h>

void util_WaitBeforeKeypress(clock_t *clockOffset, bool *keyPressed) {
    if (!(*keyPressed)) {
        while ((clock() - *clockOffset < CLOCKS_PER_SEC / 2.25) && kb_AnyKey()) {
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
