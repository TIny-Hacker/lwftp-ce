#ifndef MENU_H
#define MENU_H

#include "lwftp.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print a one-line message to the screen.
 * 
 * @param message Pointer to string to print.
 */
void menu_PrintMessage(char *message);

/**
 * @brief Show client info configuration menu.
 * 
 * @return int8_t 0 if success.
 */
int8_t menu_ClientConfig(void);

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
int8_t menu_DrawFiles(lwftp_session_t *s);

#ifdef __cplusplus
}
#endif

#endif
