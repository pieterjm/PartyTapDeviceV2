#include "network.h"
#include "bitcointap.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "ui.h"
#include "productconfig.h"
#include <HTTPUpdate.h>

TapConfig network_tapConfig;
int networkPaymentMode = PAYMENT_MODE_NONE;

#define RX_PIN 36
#define TX_PIN 37
HardwareSerial networkSerial(2);
WebSocketsClient webSocket;
bool bSerialConnected = false;


String networkPaymentRequest = "";
String networkPaymentProduct = "";
NetworkWebSocketState networkWebSocketState = WEBSOCKET_NOT_CONNECTED;
String networkPaymentHash = "";
String paymentPin = "";

int selectedProductId = 0;  // the ID of the selected product

uint8_t main_shared_lnurlw[512];
int main_shared_lnurlw_len = 0;

TimerHandle_t networkStateReportTimer;
TimerHandle_t networkCheckTimer;
TimerHandle_t networkLoopTimer;
TimerHandle_t networkInvoiceReceivedTimer;
TimerHandle_t networkInvoiceExpireTimer;

TaskHandle_t networkTaskHandle;


// ProductConfig productConfig;
// static QueueHandle_t msgQueue;


QueueHandle_t networkQueue = NULL;

struct networkQueueMessage
{
    NetworkQueueMessageType type;
    uint16_t data;
};
// network configuration and the semaphore handle to change data

// Semaphore handle for the selected product
String switchid = "";

#define MAIN_TAP_PROGRESS_INTERVAL 200

#define WS_EVENTTYPE_INVOICE "invoice"
#define WS_EVENTTYPE_PAID  "paid"
#define WS_EVENTTYPE_PAYMENTFAILED "paymentfailed"
#define WS_EVENTTYPE_SWITCHES "switches"
#define WS_EVENTTYPE_ERROR "error"
#define WS_EVENTTYPE_OPEN "open"
#define WS_EVENTTYPE_CLOSE "close"
#define WS_EVENTTYPE_STOP "stop"
#define WS_EVENTTYPE_FREE "free"


// This function is triggered by a taskscheduler at the moment an invoice is requested
// if the invoice is not received for some reason, the user is taken back to the 
// start screen with a message that no invoice was received
void check_invoice_received() {
    ui_queue_send_message(SHOW_MESSAGE_NO_INVOICE_RECEIVED,3000);
    ui_queue_send_message(LOAD_ABOUT_SCREEN,0);

}

// report the state of wifi and websocket to the UI queue
void network_report_state() {    
    debug("network_report_state","Reporting network state");
    ui_queue_send_message(REPORT_WIFI_STATE,WiFi.status());
    ui_queue_send_message(REPORT_WEBSOCKET_STATE, networkWebSocketState);
    ui_queue_send_message(REPORT_WIFI_RSSI,WiFi.RSSI());
}


void ws_lnurlw_withdraw(const char *lnurlw) {
    ui_queue_send_message(SHOW_MESSAGE_PROCESSING_MFC_PAYMENT,15000);
    String message = PSTR("{\"event\":\"lnurlw\",\"payment_request\":\""); 
    message += networkPaymentRequest;
    message += PSTR("\",\"lnurlw\":\"");
    message += lnurlw;
    message += PSTR("\"}");

    // send a message through the websocket if connected
    if ( webSocket.isConnected() ) {
        webSocket.sendTXT(message.c_str());
    } else {
        // send via the serial port
        networkSerial.println(message);
    }
}

// send a message to the UI queue
void network_queue_send_message(NetworkQueueMessageType type, uint16_t data)
{
    // debug("network_queue_send_message","adding message to the queue");
    struct networkQueueMessage msg;
    msg.type = type;
    msg.data = data;
    if ( networkQueue != NULL )  {
        xQueueSend(networkQueue, &msg, portMAX_DELAY);
    } else {
        debug("network_queue_send_message","failed to send message");

    }
}


// firmware update
bool main_firmware_update() {
    network_queue_send_message(TASK_NOTIFICATION_FIRMWARE_UPDATE,0);
    return true;
}

void firmwareUpdateFinished() {
    ui_queue_send_message(SHOW_MESSAGE_RESTARTING,0);
}

void firmwareUpdateProgress(int cur, int total) {
    ui_queue_send_message(SHOW_MESSAGE_UPDATE_PROGRESS,100 * cur / total);
}

void firmwareUpdateError(int err) {
    debug("firmwareUpdateError","error code %d",err);
    switch (err ) {
      case HTTP_UE_NO_PARTITION:
        ui_queue_send_message(SHOW_MESSAGE_NO_PARTITION,0);
        break;
      case HTTP_UE_BIN_FOR_WRONG_FLASH:
        ui_queue_send_message(SHOW_MESSAGE_WRONG_FLASH,0);
        break;
      case HTTP_UE_BIN_VERIFY_HEADER_FAILED:
        ui_queue_send_message(SHOW_MESSAGE_VERIFY_FAILED,0);
        break;
      case HTTP_UE_SERVER_FAULTY_MD5:
        ui_queue_send_message(SHOW_MESSAGE_HASH_ERROR,0);
        break;
      case HTTP_UE_SERVER_WRONG_HTTP_CODE:
        ui_queue_send_message(SHOW_MESSAGE_BAD_SERVER_RESPONSE,0);      
        break;
      case HTTP_UE_SERVER_FORBIDDEN:
        ui_queue_send_message(SHOW_MESSAGE_NOT_ALLOWED,0);
        break;
      case HTTP_UE_SERVER_FILE_NOT_FOUND:
        ui_queue_send_message(SHOW_MESSAGE_FILE_NOT_FOUND,0);
        break;
      case HTTP_UE_SERVER_NOT_REPORT_SIZE:
        ui_queue_send_message(SHOW_MESSAGE_SIZE_NOT_REPORTED,0);
        break;
      case HTTP_UE_TOO_LESS_SPACE:
        ui_queue_send_message(SHOW_MESSAGE_NOT_ENOUGH_SPACE,0);
        break;
      default:
        break;
    }
}

void firmware_update() {
    static char firmware_path[200];    

    webSocket.disableHeartbeat();
    webSocket.disconnect();

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(12000);


    httpUpdate.onEnd(firmwareUpdateFinished);
    httpUpdate.onProgress(firmwareUpdateProgress);
    httpUpdate.rebootOnUpdate(true);

#define FIRMWARE_HOST "firmware.bitcointaps.com"

    if (xSemaphoreTake(productConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        snprintf_P(firmware_path, sizeof(firmware_path), PSTR("/partytap/latest/JC3248W535EN/firmware_%s.bin"), productConfig.getServerBranding());
        xSemaphoreGive(productConfigSemaphore);
    }
  t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_HOST, 443, firmware_path);
  
  if ( ret == HTTP_UPDATE_FAILED ) {
    firmwareUpdateError(httpUpdate.getLastError());
    delay(10000);
    ESP.restart();
    return;
  }
}

void _network_send_invoice_request(int productid) {
    debug("internal_send_invoice","preparing and sending invoice request");
    xTimerReset(networkInvoiceReceivedTimer, 1000);

    String switchid = "";
    if ( xSemaphoreTake(paymentDataSemaphore,portMAX_DELAY) == pdTRUE ) {
        if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) == pdTRUE ) {
            paymentDataProduct = productConfig.getProduct(productid)->getSwitchID();
            paymentDataAmount = productConfig.getProduct(productid)->getAmount();
            paymentDataCurrency = productConfig.getProduct(productid)->getCurrency();
            xSemaphoreGive(productConfigSemaphore);        
        }
        switchid = paymentDataProduct;    
        xSemaphoreGive(paymentDataSemaphore);          
    }
            
    // create messsage
    String message = PSTR("{\"event\":\"createinvoice\",\"switch_id\":\"");
    message +=  switchid;
    message += PSTR("\"}");


    // send the message dependent on the payment mode
    switch ( networkPaymentMode ) {
        case PAYMENT_MODE_WIFI:
            webSocket.sendTXT(message.c_str());
            break;
        case PAYMENT_MODE_SERIAL:
            networkSerial.println(message);
            break;
        default:
            break;
    }
}



// called from UI thread, so lock before accessing variables
void network_connect(TapConfig c) {
    debug("network_connect","copying variables and notifying task");

    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {        
        network_tapConfig = c;
        // config_wspath = "/partytap/api/v1/ws/" + deviceid;        
        xSemaphoreGive(networkConfigSemaphore);
    }
    //xTaskNotify(networkTaskHandle, TASK_NOTIFICATION_NETWORK_RESTART, eSetBits);

    network_queue_send_message(TASK_NOTIFICATION_NETWORK_RESTART,0);
    debug("network_connect","finished");
}

void _network_wifi_stop() {
    webSocket.setReconnectInterval(60000);
    webSocket.disconnect();
    WiFi.disconnect();
}

void _network_serial_stop() {
   networkSerial.end();
   bSerialConnected = false;
}

void _network_stop() {
    _network_wifi_stop();
    _network_serial_stop();
}

// start the wifi network
void _network_wifi_start() {
    String ssid = "";
    String pwd = "";

    // lock and get data
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        ssid = network_tapConfig.getWiFiSSID();
        pwd = network_tapConfig.getWiFiPWD();
        xSemaphoreGive(networkConfigSemaphore);
    }

    // start WiFi
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.begin(ssid,pwd);
}   

// send a connect message on the serial network
void _network_serial_send_connect() {
    String message = "{\"event\":\"connect\",\"lnbitshost\":\"";
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        message += network_tapConfig.getLNbitsHost();
        message += "\",\"deviceid\":\"";
        message += network_tapConfig.getDeviceID();
        xSemaphoreGive(networkConfigSemaphore);
    }   
    message += "\"}";

    networkSerial.println(message);
}

// start the serial network
void _network_serial_start() {
    bSerialConnected = false;

    networkSerial.setRxBufferSize(2048);
    networkSerial.begin(230400, SERIAL_8N1, U0RXD_GPIO_NUM, U0TXD_GPIO_NUM);

    _network_serial_send_connect();
}
 
// start the network
void _network_start() {
    networkPaymentMode = PAYMENT_MODE_NONE;
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        networkPaymentMode = network_tapConfig.getPaymentMode();
        xSemaphoreGive(networkConfigSemaphore);
    }

    switch ( networkPaymentMode ) {
        case PAYMENT_MODE_WIFI:
            _network_wifi_start();
            break;
        case PAYMENT_MODE_SERIAL:
            _network_serial_start();
            break;
        default:
            break;    
    }
}

// stop and start the network
void _network_restart() {
    _network_stop();
    _network_start();
}

bool parse_received_text_message(const char *payload, int length)
{
    DynamicJsonDocument doc(2000);
    DeserializationError error = deserializeJson(doc, (char *)payload);
    if (error != DeserializationError::Ok)
    {
        debug("ws_event", "Deserialisation error of websocket message");
        return false;
    }
    const char *eventtype = doc["event"].as<const char *>();
    debug("ws_event", "eventtype: %s", eventtype);
    if (strcmp(eventtype, WS_EVENTTYPE_SWITCHES) == 0)
    {

        if (xSemaphoreTake(productConfigSemaphore, portMAX_DELAY) == pdTRUE)
        {
            productConfig.parse(&doc);
            productConfig.save();
            xSemaphoreGive(productConfigSemaphore);
        }

        // configure switches in the UI
        ui_queue_send_message(SHOW_PAYMENT_BUTTONS, 0);
        networkWebSocketState = WEBSOCKET_CONNECTED;
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_INVOICE) == 0)
    {
        debug("ws_event", "received an invoice");

        networkPaymentRequest = doc["pr"].as<const char *>();
        networkPaymentHash = doc["payment_hash"].as<const char *>();
        debug("pr","pr length = %d",networkPaymentRequest.length());

        // start an expire invoice task and stop the invoice received timer
        xTimerStop(networkInvoiceReceivedTimer, 1000);
        xTimerReset(networkInvoiceExpireTimer, 1000);

        // make a withdraw is lengthis larger than 0, otherwise navigate to the payment screen
        if (main_shared_lnurlw_len > 0)
        {
            ws_lnurlw_withdraw((const char *)main_shared_lnurlw);
            main_shared_lnurlw[0] = '\0';
            main_shared_lnurlw_len = 0;
        }
        else
        {
            ui_queue_send_message(HIDE_MESSAGE, 0);
        }

        float amount = 0;
        String currency = "";

        if (xSemaphoreTake(productConfigSemaphore, portMAX_DELAY) == pdTRUE)
        {
            amount = productConfig.getProduct(selectedProductId)->getAmount();
            currency = productConfig.getProduct(selectedProductId)->getCurrency();
            xSemaphoreGive(productConfigSemaphore);
        }

        debug("ws_event", "calling ui_init_and_load_payment_screen");
        ui_init_and_load_payment_screen(networkPaymentRequest, currency, amount);
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_PAID) == 0)
    {
        debug("ws_event", "got a paid notification");
        tone_paid();
        ui_queue_send_message(SHOW_MESSAGE_INVOICE_PAID, 3000);
        // expiore the invoice
        xTimerStop(networkInvoiceExpireTimer, 1000);

        // reset data
        paymentPin = "";
        networkPaymentHash = "";
        networkPaymentRequest = "";

        int tap_volume = doc["payload"].as<int>();

        ui_queue_send_message(ADD_VOLUME, tap_volume);
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_PAYMENTFAILED) == 0)
    {
        ui_queue_send_message(SHOW_MESSAGE_PAYMENT_FAILED, 5000);
        tone_paymentfailed();
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_STOP) == 0) 
    {
        ui_queue_send_message(STOP_TAP, 5000);        
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_OPEN) == 0) 
    {
        ui_queue_send_message(OPEN_TAP, 5000);        
    }
    else if ( strcmp(eventtype,WS_EVENTTYPE_FREE) == 0 ) {
        ui_queue_send_message(FREE_DRINK,0);
    }
    else if (strcmp(eventtype, WS_EVENTTYPE_ERROR) == 0)
    {
        ui_queue_send_message(SHOW_MESSAGE_WEBSOCKET_NOT_CONNECTED, 5000);

        String message = doc["message"].as<const char *>();
        if (message.equals("device id does not exist"))
        {
            networkWebSocketState = WEBSOCKET_INCORRECT_DEVICEID;
        }
        else
        {
            networkWebSocketState = WEBSOCKET_UNKNOWN_ERROR;
        }
    }

    else
    {
        debug("ws_event", "Unknown event type: '%s'", eventtype);
        return false;
    }

    return true;
}

// handle a websocket event
// This function is called by the websocket.loop function, thus semaphore locking should not be done here
void ws_event(WStype_t type, uint8_t * payload, size_t length) {
    switch ( type ) {
        case WStype_DISCONNECTED:
            debug("ws_event","Websocket disconnected"); 
            networkWebSocketState = WEBSOCKET_NOT_CONNECTED;
            break;
        case WStype_CONNECTED:
            debug("ws_event","Websocket connected"); 
            break;
        case WStype_TEXT:
            parse_received_text_message((char *)payload,length);
            break;
        case WStype_PING:
            //debug("ws_event","Received PING");
            break;
        case WStype_PONG:
            //debug("ws_event","Received PONG");
            break;
        default:
            debug("ws_event","unhandled event of type = %d\n",type);
            break;
    }
}

// expire the current available invoice
// do not lock the main_loop in this function
void expire_invoice() {
    // expire the task
    xTimerStop(networkInvoiceExpireTimer, 1000);
  
    // reset data
    paymentPin = "";
    networkPaymentHash = "";
    networkPaymentRequest = "";  
}


void _network_wifi_check_websocket() {

    if ( webSocket.isConnected() ) {
        return;
    }

    debug("checkNetwork", "webSocket is not connected");

    // ui_queue_send_message(SHOW_MESSAGE_NOT_CONNECTED,3000);
    // ui_queue_send_message(HIDE_PAYMENT_BUTTONS,0);

    webSocket.disconnect();

    String lnbitshost = "";
    String deviceid = "";
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE)
    {
        lnbitshost = network_tapConfig.getLNbitsHost();
        deviceid = network_tapConfig.getDeviceID();
        xSemaphoreGive(networkConfigSemaphore);
    }

    if ((lnbitshost.length() > 0) && (deviceid.length() > 0))
    {
        String wspath = "/partytap/api/v1/ws/" + deviceid;

        webSocket.beginSSL(lnbitshost, 443, wspath);
        webSocket.enableHeartbeat(5000, 2000, 1);
        webSocket.setReconnectInterval(500);
    }
}

// check the network and restart if needed
// this task is executed by the task scheduler, which is already locked
void _network_wifi_check()
{    
    switch ( WiFi.status() ) {
        case WL_IDLE_STATUS:
            // ui_queue_send_message(SHOW_MESSAGE_WIFI_NOT_CONNECTED,5000);
            break;
        case WL_STOPPED:
            debug("checkNetwork","Starting WiFi");\
            _network_wifi_start();
            break;
        case  WL_CONNECTED:
            debug("checkNetwork","WL_CONNECTED");
            _network_wifi_check_websocket();
            break;
        case WL_DISCONNECTED:
            debug("checkNetwork","WiFi is not connected, reconnecting");
            _network_wifi_start();
            break;
        case WL_NO_SSID_AVAIL:
            // debug("check_network","SSID not available");
            // ui_queue_send_message(SHOW_MESSAGE_SSID_NOT_FOUND,6000);
            break;
        default:
            debug("check_network","Unknown WiFi status %d",WiFi.status());
            break;
    }
}

// verify serial network status
void _network_serial_check() {
    // resend connect message on serial when not connected
    if ( bSerialConnected == false ) {
        _network_serial_send_connect();
    }
}

void _network_check() {
    if ( networkPaymentMode == PAYMENT_MODE_WIFI ) {
        _network_wifi_check();
    } else if ( networkPaymentMode == PAYMENT_MODE_SERIAL ) {
        _network_serial_check();
    }

}

void network_state_report_timer_cb(TimerHandle_t timer)
{
    network_queue_send_message(TASK_NOTIFICATION_NETWORK_STATE, 0);
}

void network_check_timer_cb(TimerHandle_t timer)
{
    network_queue_send_message(TASK_NOTIFICATION_NETWORK_CHECK, 0);
}

void network_loop_timer_cb(TimerHandle_t timer)
{
    network_queue_send_message(TASK_NOTIFICATION_NETWORK_LOOP, 0);
}

void network_invoice_received_timer_cb(TimerHandle_t timer)
{
    //network_queue_send_message(TASK_NOTIFICATION_NO_INVOICE_RECEIVED, 0);
}

void network_invoice_expire_timer_cb(TimerHandle_t timer)
{
    // xTaskNotify(networkTaskHandle, TASK_NOTIFICATION_INVOICE_EXPIRED, eSetBits);
}


void network_task(void *parameters) {    
    debug("network_task","Starting");

    struct networkQueueMessage msg;


    xTimerStart(networkCheckTimer, 1000);
    xTimerStart(networkLoopTimer,1000);
    

    webSocket.onEvent(ws_event);
            
    String networkSerialBuffer = "";

    while ( 1 == 1 ) {  
        // read all bytes from the serial port

        if ( networkPaymentMode == PAYMENT_MODE_SERIAL ) {
            while ( networkSerial.available() ) {
                char data = networkSerial.read();

                // ignore characters that are not the start of a JSON structure
                if (( networkSerialBuffer.length() == 0 ) && ( data != '{')) {
                    continue;
                }

                // read until we see a newline or carriage return and parse the buffer
                if (( data == '\n' ) || ( data == '\r')) {
                    debug("network_task","received a message of length %d",networkSerialBuffer.length());
                    if ( parse_received_text_message(networkSerialBuffer.c_str(),networkSerialBuffer.length()) ) {
                        bSerialConnected = true;
                    }
                    networkSerialBuffer = "";
                } else {
                    // add data to the buffer
                    networkSerialBuffer += data;
                }
            }
        }

         // check the queue
        if (xQueueReceive(networkQueue, &msg, 10))
        {
            switch (msg.type) {
                case TASK_NOTIFICATION_NETWORK_STATE:
                    network_report_state();
                    break;
                case TASK_NOTIFICATION_NETWORK_CHECK:
                    _network_check();
                    break;
                case TASK_NOTIFICATION_NETWORK_RESTART:
                    _network_restart();                    
                    break;
                case TASK_NOTIFICATION_NETWORK_LOOP:
                    if ( networkPaymentMode == PAYMENT_MODE_WIFI ) {
                        webSocket.loop();
                    }
                    break;
                case TASK_NOTIFICATION_FIRMWARE_UPDATE:
                    firmware_update(); 
                    break;
                case TASK_NOTIFICATION_REQUEST_INVOICE:
                    debug("network_task","send invoice");
                    _network_send_invoice_request((int)msg.data);
                    break;
                default:
                    debug("network_task","Unknown message type");
                    break;
            }
        }
    }

        // // no invoice received on time
        // if ( ulNotifiedValue & TASK_NOTIFICATION_NO_INVOICE_RECEIVED ) {
        //     ui_queue_send_message(SHOW_MESSAGE_NO_INVOICE_RECEIVED,5000);            
        // }

        // // invoice expired
        // if ( ulNotifiedValue & TASK_NOTIFICATION_INVOICE_EXPIRED ) {
        //     expire_invoice();            
        //     ui_queue_send_message(LOAD_ABOUT_SCREEN,0);
        // }

}


// called when something succesfull was read from the NFC sensor
void event_nfc_data_read(const uint8_t *buffer,int num) {
    debug("event_nfc_data_read","");
        ui_queue_send_message(SHOW_MESSAGE_NFC_READ_SUCCES,4000);


        // if there is an invoice availble, do the withdraw now, otherwise, requiest an invoice
        if ( networkPaymentRequest.length() > 0 ) {           
            // there is a payment request, do the withdraw now                         
            ws_lnurlw_withdraw((const char *)buffer);             
        } else {
            // there is no payment request, copy the lnurlw
            memcpy(main_shared_lnurlw,buffer,num);
            main_shared_lnurlw_len = num;

            // and request an invoice
            network_queue_send_message(TASK_NOTIFICATION_REQUEST_INVOICE,0);
        } 
        
}





void network_task_init() {
    debug("network_task_init","Initializing network");

    // create queue for added volume
    networkQueue = xQueueCreate(5, sizeof(networkQueueMessage));
    if (networkQueue == NULL)
    {
        debug("network_task_init", "failed to created networkQueue");
    }


    // create a timer for providing regular updates of the network state
    networkStateReportTimer = xTimerCreate("network state report timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, network_state_report_timer_cb);
    if (!networkStateReportTimer) {
        debug("network_init", "Could not create network state report timer");
    }

    // create a timer when no invoice has been received
    networkInvoiceReceivedTimer = xTimerCreate("network invoice received timer", pdMS_TO_TICKS(15000), pdFALSE, NULL, network_invoice_received_timer_cb);
    if (!networkInvoiceReceivedTimer) {
        debug("network_init", "Could not create network invoice received timer.");
    }

    // create a timer when the current invoice is to be expired
    networkInvoiceExpireTimer = xTimerCreate("network invoice expire timer", pdMS_TO_TICKS(120000), pdFALSE, NULL, network_invoice_expire_timer_cb);
    if (!networkInvoiceExpireTimer) {
        debug("network_init", "Could not create network invoice expire timer.");
    }

    // create a timer to check the status of the network and the websocket
    networkCheckTimer = xTimerCreate("network check timer", pdMS_TO_TICKS(5000), pdTRUE, NULL, network_check_timer_cb);
    if ( !networkCheckTimer) {
        debug("network_init", "Could not create network check timer");
    }

    // create a timer to check the status of the network and the websocket
    networkLoopTimer = xTimerCreate("network loop timer", 100, pdTRUE, NULL, network_loop_timer_cb);
    if ( ! networkLoopTimer) {
        debug("network_init", "Could not create network loop timer");
    }




}

void network_task_start() {

    if ( xTaskCreatePinnedToCore (
        network_task,     // Function to implement the task
        "main_loop",   // Name of the task
        10000,      // Stack size in bytes
        NULL,      // Task input parameter
        10,         // Priority of the task
        &networkTaskHandle,      // Task handle.
        0          // Core where the task should ru
     ) ) {
        debug("setup","network loop started");
    } else {
        debug("setup","Failed to start network loop");
    }
}
