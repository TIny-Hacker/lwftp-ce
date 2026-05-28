#ifndef UTILITY_H
#define UTILITY_H

#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle USB network events.
 * 
 */
void util_ServiceNetwork(void);

/**
 * @brief Waits before repeating a keypress.
 * 
 * @param clockOffset Clock offset for timer.
 * @param keyPressed Whether a key is currently pressed.
 */
void util_WaitBeforeKeypress(clock_t *clockOffset, bool *keyPressed);

/**
 * @brief Parse string to 4-byte address array.
 * 
 * @param s Input string.
 * @param addr Output array.
 * @return int8_t 0 if successful.
 */
int8_t util_ParseAddr(char *s, uint8_t *addr);

/**
 * @brief Scans the keypad and returns a character based on the key pressed.
 * 
 * @param inputMode Input mode, like 2nd mode, uppercase mode, or lowercase mode.
 * @return char Character found from keypress.
 */
char asm_util_GetCharFromKey(uint8_t inputMode);

/**
 * @brief Read config from AppVar.
 * 
 */
void util_ReadConfig(void);

/**
 * @brief Write config to AppVar.
 * 
 */
void util_WriteConfig(void);

/**
 * @brief Scan all local AppVars and programs and save them to pixelShadow.
 * 
 */
void util_GetLocalFiles(void);

/**
 * @brief Begin scanning files in the current remote directory.
 * 
 * @param s lwftp session.
 */
void util_GetRemoteFiles(lwftp_session_t *s);

/**
 * @brief Begin getting the current working directory and storing it to app.path.
 * 
 * @param s lwftp session.
 */
void util_GetDir(lwftp_session_t *s);

/**
 * @brief Change the current working directory.
 * 
 * @param s lwftp session.
 */
void util_ChangeDir(lwftp_session_t *s);

/**
 * @brief Delete the currently selected file.
 * 
 * @param s lwftp session.
 */
void util_DeleteFile(lwftp_session_t *s);

/**
 * @brief Move the currently selected file.
 * 
 * @param s lwftp session.
 */
void util_MoveFile(lwftp_session_t *s);

#ifdef __cplusplus
}
#endif

#endif
