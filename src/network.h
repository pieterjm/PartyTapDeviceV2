#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <Arduino.h>
#include <tapconfig.h>

void send_invoice_request(int product);

void event_nfc_data_read(const uint8_t *buffer,int num);

bool main_firmware_update();

void network_connect(TapConfig c); 

void network_task(void *parameters);

void ws_lnurlw_withdraw(const char *lnurlw);

void network_task_init();

void network_task_start();




extern TimerHandle_t networkStateReportTimer;

enum NetworkWebSocketState
{
    WEBSOCKET_NOT_CONNECTED = 0x1,
    WEBSOCKET_CONNECTED,
    WEBSOCKET_INCORRECT_DEVICEID,
    WEBSOCKET_UNKNOWN_ERROR
};

// the following list of variables can only be accessed when a lock
enum NetworkQueueMessageType
{
    TASK_NOTIFICATION_NETWORK_STATE = 0x1,
    TASK_NOTIFICATION_NETWORK_CHECK,
    TASK_NOTIFICATION_NETWORK_RESTART,
    TASK_NOTIFICATION_NETWORK_LOOP,
    TASK_NOTIFICATION_FIRMWARE_UPDATE,
    TASK_NOTIFICATION_REQUEST_INVOICE
};

void network_queue_send_message(NetworkQueueMessageType type, uint16_t data);

#endif
