#ifndef _UI_H_
#define _UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "tapconfig.h"

void ui_init(void);
void ui_loop(void *parameters);

bool ui_hide_payment_buttons();
bool ui_admin_screen_active();
bool ui_update_wifi_state(const char *s);
bool ui_update_websocket_state(const char *s);
bool ui_update_server_version(const char *s);
bool ui_update_server_branding(const char *s);
bool ui_set_payment_message(const char *s,int delay);
bool ui_hide_payment_message();
bool ui_load_about_screen(); 
bool ui_load_payment_screen(int amount, const char *currency);
bool ui_init_switches();
bool ui_display_qrcode(const char *s,int len);
bool ui_add_tap_volume(int credit);
bool ui_payment_screen_active();

// the following list of variables can only be accessed when a lock
#define UIQUEUE_ADD_VOLUME 0x1
#define UIQUEUE_WIFI_STATE 0x2
#define UIQUEUE_WS_STATE 0x3
#define UIQUEUE_SERVER_VERSION 0x4
#define UIQUEUE_SERVER_BRANDING 0x5
#define UIQUEUE_MESSAGE_NO_INVOICE_RECEIVED 0x6
#define UIQUEUE_MESSAGE_PROCESSING_NFC_PAYMENT 0x7
#define UIQUEUE_MESSAGE_RESTARTING 0x8
#define UIQUEUE_MESSAGE_UPDATE_PROGRESS 0x9
#define UIQUEUE_MESSAGE_NO_PARTITION 0x10
#define UIQUEUE_MESSAGE_WRONG_FLASH 0x11
#define UIQUEUE_MESSAGE_VERIFY_FAILED 0x12
#define UIQUEUE_MESSAGE_HASH_ERROR 0x13
#define UIQUEUE_MESSAGE_BAD_SERVER_RESPONSE 0x14
#define UIQUEUE_MESSAGE_NOT_ALLOWED 0x15
#define UIQUEUE_MESSAGE_FILE_NOT_FOUND 0x16
#define UIQUEUE_MESSAGE_SIZE_NOT_REPORTED 0x17
#define UIQUEUE_MESSAGE_NOT_ENOUGH_SPACE 0x18
#define UIQUEUE_MESSAGE_INVOICE_PAID 0x19
#define UIQUEUE_MESSAGE_PAYMENT_FAILED 0x20
#define UIQUEUE_MESSAGE_NFC_READ_SUCCES 0x21
#define UIQUEUE_MESSAGE_WIFI_NOT_CONNECTED 0x22
#define UIQUEUE_MESSAGE_WEBSOCKET_NOT_CONNECTED 0x23
#define UIQUEUE_MESSAGE_READING_NFC 0x24
#define UIQUEUE_MESSAGE_NFC_READ_ERROR 0x25
#define UIQUEUE_MESSAGE_INVALID_CARD 0x26
#define UIQUEUE_MESSAGE_CREATING_INVOICE 0x27
#define UIQUEUE_DISPLAY_PAYMENT_REQUEST 0x28
#define UIQUEUE_DISPLAY_PAYMENT_MESSAGE 0x29
#define UIQUEUE_HIDE_PAYMENT_BUTTONS 0x30
#define UIQUEUE_INIT_PAYMENT_BUTTONS 0x31
#define UIQUEUE_LOAD_ABOUT_SCREEN 0x32


#ifdef __cplusplus
}
#endif



#endif