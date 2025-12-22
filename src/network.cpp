#include "network.h"
#include "bitcointap.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include "ui.h"
#include "productconfig.h"
#include <HTTPUpdate.h>



String payment_request = "";
String payment_hash = "";
String payment_pin = "";
int selected_product_id = 0;  // the ID of the selected product

uint8_t main_shared_lnurlw[512];
int main_shared_lnurlw_len = 0;

TimerHandle_t networkStateReportTimer;
TimerHandle_t networkCheckTimer;
TimerHandle_t networkLoopTimer;
TaskHandle_t networkTask_Handle;

#define TASK_NOTIFICATION_NETWORK_STATE (1 << 1)
#define TASK_NOTIFICATION_NETWORK_RESTART (1 << 2)
#define TASK_NOTIFICATION_NETWORK_LOOP (1 << 3)
#define TASK_NOTIFICATION_NETWORK_CHECK (1 << 4)


bool bDoFirmwareUpdate = false;

// ProductConfig productConfig;
// static QueueHandle_t msgQueue;

WebSocketsClient webSocket;


// network configuration and the semaphore handle to change data
SemaphoreHandle_t networkConfigSemaphore;
String config_wifi_pwd = "";
String config_wifi_ssid = "";
String config_wspath = "";
String config_lnbitshost = "";

void expireInvoice();
void checkInvoiceReceived();
void firmwareUpdate();
Task expireInvoiceTask(TASK_IMMEDIATE,TASK_ONCE,&expireInvoice);
Task checkInvoiceReceivedTask(TASK_IMMEDIATE, TASK_ONCE,&checkInvoiceReceived);
Task firmwareUpdateTask(TASK_SECOND, TASK_FOREVER, &firmwareUpdate);

#define MAIN_TAP_PROGRESS_INTERVAL 200

#define WS_EVENTTYPE_INVOICE "invoice"
#define WS_EVENTTYPE_PAID  "paid"
#define WS_EVENTTYPE_PAYMENTFAILED "paymentfailed"
#define WS_EVENTTYPE_SWITCHES "switches"


// This function is triggered by a taskscheduler at the moment an invoice is requested
// if the invoice is not received for some reason, the user is taken back to the 
// start screen with a message that no invoice was received
void checkInvoiceReceived() {
    ui_queue_send_message(SHOW_MESSAGE_NO_INVOICE_RECEIVED,3000);
    ui_queue_send_message(LOAD_ABOUT_SCREEN,0);

}

// report the state of wifi and websocket to the UI queue
void networkReportState() {    
    ui_queue_send_message(REPORT_WIFI_STATE,WiFi.status());
    ui_queue_send_message(REPORT_WEBSOCKET_STATE, webSocket.isConnected());
}


void ws_lnurlw_withdraw(const char *lnurlw) {
    ui_queue_send_message(SHOW_MESSAGE_PROCESSING_MFC_PAYMENT,15000);
    String wsmessage = PSTR("{\"event\":\"lnurlw\",\"payment_request\":\""); 
    wsmessage += payment_request;
    wsmessage += PSTR("\",\"lnurlw\":\"");
    wsmessage += lnurlw;
    wsmessage += PSTR("\"}");
    webSocket.sendTXT(wsmessage);  
}

// firmware update
bool main_firmware_update() {
    bDoFirmwareUpdate = true;
    firmwareUpdateTask.restartDelayed(2000);
    return true;
}

void firmwareUpdateFinished() {
    ui_queue_send_message(SHOW_MESSAGE_RESTARTING,0);
}

void firmwareUpdateProgress(int cur, int total) {
    ui_queue_send_message(SHOW_MESSAGE_UPDATE_PROGRESS,100 * cur / total);
}

void firmwareUpdateError(int err) {
    Serial.printf("firmwareUpdateError %d\n",err);
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

void firmwareUpdate() {
    Serial.println("[firmwareUpdate]");
    static char firmware_path[200];    
    if ( ! bDoFirmwareUpdate ) {
        return;
    }
    bDoFirmwareUpdate = false;
    
    Serial.println("[firmwareUpdate] after checks");
    webSocket.disableHeartbeat();
    webSocket.disconnect();

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(12000);


    httpUpdate.onEnd(firmwareUpdateFinished);
    httpUpdate.onProgress(firmwareUpdateProgress);
    httpUpdate.onError(firmwareUpdateError);
    httpUpdate.rebootOnUpdate(true);

#define FIRMWARE_HOST "firmware.bitcointaps.com"
  snprintf_P(firmware_path, sizeof(firmware_path), PSTR("/partytap/latest/JC3248W535EN/firmware_%s.bin"), productConfig.getServerBranding());
#ifdef DEBUG
  Serial.printf("[doFirmwareUpdate] Firmware path %s\n",firmware_path);
#endif
  t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_HOST, 443, firmware_path);
  
  if ( ret == HTTP_UPDATE_FAILED ) {
#ifdef DEBUG
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
#endif
    firmwareUpdateError(httpUpdate.getLastError());
    delay(10000);
    ESP.restart();
    return;
  }
}

// send a websocket event to request an invoice
// running in the main loop, do not lock here
void ws_request_invoice(String switchid) {
#ifdef DEBUG
    Serial.println("[ws_request_invoice]");
#endif
    checkInvoiceReceivedTask.restartDelayed(TASK_SECOND * 15);

    String wsmessage = PSTR("{\"event\":\"createinvoice\",\"switch_id\":\"");
    wsmessage +=  switchid;
    wsmessage += PSTR("\"}");
    if ( webSocket.sendTXT(wsmessage) ) {
        Serial.println("Websocket send succes");
    } else {
        Serial.println("Websocket send failure");
    }
}




// called from UI thread, so lock before accessing variables
void main_get_invoice(int product) {
    int i;

    selected_product_id = product;

    String switchId = "";
    if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) == pdTRUE ) {
        switchId = productConfig.getProduct(selected_product_id)->getSwitchID();
        xSemaphoreGive(productConfigSemaphore);        
    }


    ws_request_invoice(switchId);
}

// called from UI thread, so lock before accessing variables
void network_restart(String wifi_ssid,String wifi_pwd,String lnbitshost,String deviceid) {
    debug("network_restart","Restarting network");
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        config_wifi_ssid = wifi_ssid;
        config_wifi_pwd = wifi_pwd;
        config_lnbitshost = lnbitshost;
        config_wspath = "/partytap/api/v1/ws/" + deviceid;        
        xSemaphoreGive(networkConfigSemaphore);
    }
}


void internal_network_restart() {
    webSocket.setReconnectInterval(60000);
    webSocket.disconnect();
    WiFi.disconnect();
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
        WiFi.begin(config_wifi_ssid,config_wifi_pwd);
        xSemaphoreGive(networkConfigSemaphore);
    }

}


// handle a websocket event
// This function is called by the websocket.loop function, thus semaphore locking should not be done here
void ws_event(WStype_t type, uint8_t * payload, size_t length) {
    switch ( type ) {
        case WStype_DISCONNECTED:
            debug("ws_event","Websocket disconnected"); 
            break;
        case WStype_CONNECTED:
            debug("ws_event","Websocket connected"); 
            break;
        case WStype_TEXT:
            {
                DynamicJsonDocument doc(2000);
                DeserializationError error = deserializeJson(doc, (char *)payload);
                if ( error != DeserializationError::Ok ) {
                    debug("ws_event","Deserialisation error of websocket message");
                    return;
                }
                const char *eventtype = doc["event"].as<const char *>();
                debug("ws_event","eventtype: %s",eventtype);
                if ( strcmp(eventtype,WS_EVENTTYPE_SWITCHES) == 0 ) {


                    if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) == pdTRUE ) {                    
                        productConfig.parse(&doc);
                        productConfig.save();
                        xSemaphoreGive(productConfigSemaphore);        
                    }


                    // configure switches in the UI
                    ui_queue_send_message(SHOW_PAYMENT_BUTTONS,0);
                } else if ( strcmp(eventtype,WS_EVENTTYPE_INVOICE) == 0 ) {


                    payment_request = doc["pr"].as<const char *>();
                    payment_hash = doc["payment_hash"].as<const char *>();

                    // start an expire invoice task
                    checkInvoiceReceivedTask.disable();
                    expireInvoiceTask.restartDelayed(TASK_SECOND * 120);
                        
                    // make a withdraw is lengthis larger than 0, otherwise navigate to the payment screen
                    if ( main_shared_lnurlw_len > 0 ) {                 
                        ws_lnurlw_withdraw((const char *)main_shared_lnurlw); 
                        main_shared_lnurlw[0] = '\0';
                        main_shared_lnurlw_len = 0;                     
                    } else {
                        ui_queue_send_message(HIDE_MESSAGE,0);
                    }
                    

                    int amount = 0;
                    String currency = "";

                    if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) == pdTRUE ) {                   
                        amount = productConfig.getProduct(selected_product_id)->getAmount();
                        currency = productConfig.getProduct(selected_product_id)->getCurrency();
                        xSemaphoreGive(productConfigSemaphore);        
                    }

                    
                    ui_load_payment_screen(amount, currency.c_str());
                    ui_display_qrcode(payment_request.c_str(),payment_request.length());
                    
                } else if ( strcmp(eventtype,WS_EVENTTYPE_PAID) == 0 ) {
                    tone_paid();
                    ui_queue_send_message(SHOW_MESSAGE_INVOICE_PAID,3000);
                    // expiore the invoice
                    expireInvoiceTask.disable();
  
  
                    // reset data
                    payment_pin = "";
                    payment_hash = "";
                    payment_request = "";
                    
                    int tap_volume = doc["payload"].as<int>();

                    ui_queue_send_message(ADD_VOLUME,tap_volume);
                    
                } else if ( strcmp(eventtype,WS_EVENTTYPE_PAYMENTFAILED) == 0 ) {   
                    ui_queue_send_message(SHOW_MESSAGE_PAYMENT_FAILED,5000);
                    tone_paymentfailed();
                } else {
                    Serial.println("another");
                }
            }
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
void expireInvoice() {
#ifdef DEBUG
    Serial.println("[expireInvoice]");
#endif

    // expire the task    network_restart(ui_tapConfig.getWiFiSSID(),ui_tapConfig.getWiFiPWD(),ui_tapConfig.getLNbitsHost(),ui_tapConfig.getDeviceID());

    expireInvoiceTask.disable();
  
    // reset data
    payment_pin = "";
    payment_hash = "";
    payment_request = "";  
}


// check the network and restart if needed
// this task is executed by the task scheduler, which is already locked
void network_check()
{
    switch ( WiFi.status() ) {
        case WL_IDLE_STATUS:
            ui_queue_send_message(SHOW_MESSAGE_WIFI_NOT_CONNECTED,5000);
            break;
        case WL_STOPPED:
            debug("checkNetwork","Starting WiFi");
            WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);

            if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
                WiFi.begin(config_wifi_ssid,config_wifi_pwd);
                xSemaphoreGive(networkConfigSemaphore);
            }   
            break;
        case  WL_CONNECTED:
            debug("checkNetwork","WL_CONNECTED");
            if ( ! webSocket.isConnected() ) {
                debug("checkNetwork","webSocket is not connected");

                ui_queue_send_message(SHOW_MESSAGE_NOT_CONNECTED,3000);
                ui_queue_send_message(HIDE_PAYMENT_BUTTONS,0);

                webSocket.disconnect();

                if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
                    if (( config_lnbitshost.length() > 0 ) && ( config_wspath.length() > 0 )) {
                        debug("checkNetwork","Connecting websocket to %s%s",config_lnbitshost.c_str(),config_wspath.c_str());
                        webSocket.beginSSL(config_lnbitshost,443, config_wspath);                
                        webSocket.enableHeartbeat(5000,2000,1);
                        webSocket.setReconnectInterval(500);
                    } else {
                        debug("checkNetwork","No websocket config, not connecting");
                    }
                    xSemaphoreGive(networkConfigSemaphore);
                }
            } else {
                debug("checkNetwork","webSocket is connected");
            }
            break;
        case WL_DISCONNECTED:
            debug("checkNetwork","WiFi is not connected, reconnecting");
            if (xSemaphoreTake(networkConfigSemaphore, portMAX_DELAY) == pdTRUE) {
                WiFi.begin(config_wifi_ssid,config_wifi_pwd);
                xSemaphoreGive(networkConfigSemaphore);
            }
            break;
        case WL_NO_SSID_AVAIL:
            debug("check_network","SSID not available");
            break;
        default:
            debug("check_network","Unknown WiFi status %d",WiFi.status());

            break;
    }

}


void network_state_report_timer_cb(TimerHandle_t timer)
{
    xTaskNotify(networkTask_Handle, TASK_NOTIFICATION_NETWORK_STATE, eSetBits);
}

void network_check_timer_cb(TimerHandle_t timer)
{
    xTaskNotify(networkTask_Handle, TASK_NOTIFICATION_NETWORK_CHECK, eSetBits);
}

void network_loop_timer_cb(TimerHandle_t timer)
{
    xTaskNotify(networkTask_Handle, TASK_NOTIFICATION_NETWORK_LOOP, eSetBits);
}




void main_loop(void *parameters) {    
    debug("main_loop","Starting");


    Scheduler taskScheduler;

    taskScheduler.addTask(expireInvoiceTask);
    taskScheduler.addTask(firmwareUpdateTask);



    webSocket.onEvent(ws_event);
            

    // start the network task
#ifdef DEBUG
    Serial.println("[main_loop] starting network task");
#endif

    // create a timer for providing regular updates of the network state
    networkStateReportTimer = xTimerCreate("network state report timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, network_state_report_timer_cb);
    if (!networkStateReportTimer)
    {
        debug("network_init", "Could not create network state report timer");
    }

    // create a timer to check the status of the network and the websocket
    networkCheckTimer = xTimerCreate("network check timer", pdMS_TO_TICKS(5000), pdTRUE, NULL, network_check_timer_cb);
    if (!networkCheckTimer)
    {
        debug("network_init", "Could not create network check timer");
    }

    // create a timer to check the status of the network and the websocket
    networkLoopTimer = xTimerCreate("network loop timer", 100, pdTRUE, NULL, network_loop_timer_cb);
    if (!networkLoopTimer)
    {
        debug("network_init", "Could not create network loop timer");
    }


    while ( 1 == 1 ) {  
        uint32_t ulNotifiedValue = 0;

        xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);

        // report the state of the network
        if (ulNotifiedValue & TASK_NOTIFICATION_NETWORK_STATE )
        {
            networkReportState();
        }

        // check the state of the network
        if (ulNotifiedValue & TASK_NOTIFICATION_NETWORK_CHECK )
        {
            network_check();
        }

        // restart the network
        if (ulNotifiedValue & TASK_NOTIFICATION_NETWORK_RESTART )
        {
            internal_network_restart();
        }


        // keep network processes up and running
        if (ulNotifiedValue & TASK_NOTIFICATION_NETWORK_LOOP )
        {
            taskScheduler.execute();
            webSocket.loop();        
        }


    }
}


// called when something succesfull was read from the NFC sensor
void event_nfc_data_read(const uint8_t *buffer,int num) {
    debug("event_nfc_data_read","");
        ui_queue_send_message(SHOW_MESSAGE_NFC_READ_SUCCES,4000);


        // if there is an invoice availble, do the withdraw now, otherwise, requiest an invoice
        if ( payment_request.length() > 0 ) {           
            // there is a payment request, do the withdraw now                         
            ws_lnurlw_withdraw((const char *)buffer);             
        } else {
            // there is no payment request, copy the lnurlw
            memcpy(main_shared_lnurlw,buffer,num);
            main_shared_lnurlw_len = num;

            // and request an invoice

            String switchId = "";
            if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) == pdTRUE ) {
                switchId = productConfig.getProduct(0)->getSwitchID();
                xSemaphoreGive(productConfigSemaphore);        
            }
            ws_request_invoice(switchId);
        } 
        
}





void network_init() {
    debug("network_init","Initializing network");

    if ( xTaskCreatePinnedToCore (
        main_loop,     // Function to implement the task
        "main_loop",   // Name of the task
        10000,      // Stack size in bytes
        NULL,      // Task input parameter
        10,         // Priority of the task
        &networkTask_Handle,      // Task handle.
        0          // Core where the task should ru
     ) ) {
        debug("setup","network loop started");
    } else {
        debug("setup","Failed to start network loop");
    }
}
