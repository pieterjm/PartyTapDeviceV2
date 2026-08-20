#ifndef _UI_H_
#define _UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "tapconfig.h"


// the following list of variables can only be accessed when a lock
enum UIQueueMessageType
{
    ADD_VOLUME = 0x1,
    SHOW_PAYMENT_BUTTONS,
    HIDE_PAYMENT_BUTTONS,
    LOAD_ABOUT_SCREEN,
    INIT_AND_LOAD_PAYMENT_SCREEN,
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
    SHOW_MESSAGE_CREATING_INVOICE,
    SHOW_MESSAGE_SSID_NOT_FOUND,
    REPORT_WIFI_STATE,
    REPORT_WEBSOCKET_STATE,
    STOP_TAP,
    OPEN_TAP,
    CLOSE_TAP,
    TAP_TIMEOUT,
    FREE_DRINK    
};

// send a message to the ui queue 
void ui_queue_send_message(UIQueueMessageType type,uint16_t data);

// initialiase the use task
void ui_task_init();

// start ui task
void ui_task_start();

// request to initialise and load the payment screen
// pr is the payment request
// currency is the currency of the payment
// amount is the amount to be paid in the selected currency
bool ui_init_and_load_payment_screen(String pr, String currency, float amount);


#ifdef __cplusplus
}
#endif



#endif