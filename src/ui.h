#ifndef _UI_H_
#define _UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "tapconfig.h"

void ui_init(void);
void ui_loop(void *parameters);

bool ui_admin_screen_active();
bool ui_update_wifi_state(const char *s);
bool ui_update_websocket_state(const char *s);
bool ui_update_server_version(const char *s);
bool ui_update_server_branding(const char *s);
bool ui_load_payment_screen(int amount, const char *currency);
bool ui_display_qrcode(const char *s,int len);
bool ui_add_tap_volume(int credit);
bool ui_payment_screen_active();

// the following list of variables can only be accessed when a lock
enum UIQueueMessageType
{
    ADD_VOLUME = 0x1,
    SHOW_PAYMENT_BUTTONS,
    HIDE_PAYMENT_BUTTONS,
    LOAD_ABOUT_SCREEN,
    HIDE_MESSAGE,
    SHOW_MESSAGE_NOT_CONNECTED,
    SHOW_MESSAGE_NO_INVOICE_RECEIVED,
    SHOW_MESSAGE_PROCESSING_MFC_PAYMENT,
    SHOW_MESSAGE_RESTARTING,
    SHOW_MESSAGE_UPDATE_PROGRESS,
    SHOW_MESSAGE_NO_PARTITION,
    SHOW_MESSAGE_WRONG_FLASH,
    SHOW_MESSAGE_VERIFY_FAILED,
    SHOW_MESSAGE_HASH_ERROR,
    SHOW_MESSAGE_BAD_SERVER_RESPONSE,
    SHOW_MESSAGE_NOT_ALLOWED,
    SHOW_MESSAGE_FILE_NOT_FOUND,
    SHOW_MESSAGE_SIZE_NOT_REPORTED,
    SHOW_MESSAGE_NOT_ENOUGH_SPACE,
    SHOW_MESSAGE_INVOICE_PAID,
    SHOW_MESSAGE_PAYMENT_FAILED,
    SHOW_MESSAGE_NFC_READ_SUCCES,
    SHOW_MESSAGE_WIFI_NOT_CONNECTED,
    SHOW_MESSAGE_WEBSOCKET_NOT_CONNECTED,
    SHOW_MESSAGE_READING_NFC,
    SHOW_MESSAGE_NFC_READ_ERROR,
    SHOW_MESSAGE_INVALID_CARD,
    SHOW_MESSAGE_CREATING_INVOICE
};

void ui_queue_send_message(UIQueueMessageType type,uint16_t data);


#define UIQUEUE_ADD_VOLUME 0x1
#define UIQUEUE_WIFI_STATE 0x2
#define UIQUEUE_WS_STATE 0x3
#define UIQUEUE_SERVER_VERSION 0x4
#define UIQUEUE_SERVER_BRANDING 0x5
#define UIQUEUE_DISPLAY_PAYMENT_REQUEST 0x28
#define UIQUEUE_DISPLAY_PAYMENT_MESSAGE 0x29
#define UIQUEUE_HIDE_PAYMENT_BUTTONS 0x30


#ifdef __cplusplus
}
#endif



#endif