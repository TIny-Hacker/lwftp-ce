#ifndef MENU_H
#define MENU_H

#include "lwftp.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Very simple text input routine. Could probably be improved later.
 * 
 * @param x Top-left x coordinate.
 * @param y Top-left y coordinate.
 * @param stringLength Maximum input width in pixels.
 * @param input Input buffer.
 * @return int8_t 0 if successful.
 */
int8_t menu_StringInput(unsigned int x, uint8_t y, unsigned int width, char *input);

/**
 * @brief Print a one-line message to the screen.
 * 
 * @param message Pointer to string to print.
 */
void menu_PrintMessage(char *message);

/**
 * @brief Show server info configuration menu.
 * 
 * @param server Server address.
 * @param user Username string.
 * @param pass Password string.
 * @return int8_t 0 if success.
 */
int8_t menu_ServerConfig(uint8_t *server, char *user, char *pass);

/**
 * @brief Draw the main file explorer interface.
 * 
 * @param s lwftp session struct.
 * @return int8_t 0 if success.
 */
int8_t menu_UpdateMain(lwftp_session_t *s);

#ifdef __cplusplus
}
#endif

#endif
