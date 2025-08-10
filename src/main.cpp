// Add flow meter
// Add valve
#define DEBUG
// #define NOTONE

#include <Arduino.h>
#include <Adafruit_PN532_NTAG424.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include "ui.h"
#include "bitcointap.h"
#include "productconfig.h"
#include <LittleFS.h>
#include <BackgroundAudioWAV.h>
#include <ESP32I2SAudio.h>
#include "pingwav.h"
#include "levelup.h"
#include "FreeRTOS.h"
#include <HTTPUpdate.h>




static SemaphoreHandle_t nfc_sem;
static SemaphoreHandle_t control_sem;


#define AUDIO_I2S_PORT I2S_NUM_0
#define AUDIO_I2S_MCK_IO -1 // MCK
#define AUDIO_I2S_BCK_IO 42 // BCK
#define AUDIO_I2S_LRCK_IO 2   // LCK
#define AUDIO_I2S_DO_IO 41  // DIN


ESP32I2SAudio audio(AUDIO_I2S_BCK_IO,AUDIO_I2S_LRCK_IO,AUDIO_I2S_DO_IO,AUDIO_I2S_MCK_IO);
ROMBackgroundAudioWAV BMP(audio);

TaskHandle_t nfcTask_Handle;
TaskHandle_t lvglTask_Handle;
TaskHandle_t controlTask_Handle;

String payment_request = "";
String payment_hash = "";
String payment_pin = "";
String config_wspath = "";
uint8_t main_shared_lnurlw[512];
int main_shared_lnurlw_len = 0;
bool bDoFirmwareUpdate = false;

ProductConfig productConfig;
TapConfig main_shared_tapConfig;

#define SENSACT_NFC_CB_UNAVAILABLE  0
#define SENSACT_NFC_CB_NOREAD       1
#define SENSACT_NFC_CB_INCOMPATIBLE 2
#define SENSACT_NFC_CB_NO_NTAG424   3
#define SENSACT_NFC_CB_NO_BYTES     4
#define SENSACT_NFC_CB_READ_SUCCESS 5
#define SENSACT_NFC_CB_NO_LNURLW    6
#define SENSACT_NFC_CB_NTAG424      7
#define SENSACT_NFC_CB_READING    8

#define MAIN_TAP_PROGRESS_INTERVAL 200

#define WS_EVENTTYPE_INVOICE "invoice"
#define WS_EVENTTYPE_PAID  "paid"
#define WS_EVENTTYPE_PAYMENTFAILED "paymentfailed"
#define WS_EVENTTYPE_SWITCHES "switches"

void ws_request_invoice(String switchid);


void checkNetwork();
void reportState();
// void updateTapProgress();
void expireInvoice();
void checkInvoiceReceived();
void firmwareUpdate();
Task checkNetworkTask(5 * TASK_SECOND, TASK_FOREVER,&checkNetwork);
Task reportStateTask(2 * TASK_SECOND, TASK_FOREVER,&reportState);
// Task tapProgressTask(TASK_IMMEDIATE, TASK_FOREVER, &updateTapProgress);
Task expireInvoiceTask(TASK_IMMEDIATE,TASK_ONCE,&expireInvoice);
Task checkInvoiceReceivedTask(TASK_IMMEDIATE, TASK_ONCE,&checkInvoiceReceived);
Task firmwareUpdateTask(TASK_SECOND, TASK_FOREVER, &firmwareUpdate);


// static QueueHandle_t msgQueue;

WebSocketsClient webSocket;



TwoWire *wire;
Adafruit_PN532 *pn532 = NULL;

#if LV_USE_LOG != 0
// Log to serial console
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif


void tone_negative() {
#ifdef NOTONE
    return;
#endif
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.write(levelup_wav, sizeof(levelup_wav));
}

void tone_positive() {
#ifdef NOTONE
    return;
#endif
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.write(pingwav, sizeof(pingwav));
}

void tone_switches() {
    tone_positive();
}

void tone_online() {
    tone_positive();
}

void tone_offline() {
    tone_negative();
}


void tone_paid() {
    tone_positive();
}

void tone_paymentfailed() {
    tone_negative();
}


void internal_set_message(const char *s,int delay) {
    ui_set_payment_message(s, delay);
}

// This function is triggered by a taskscheduler at the moment an invoice is requested
// if the invoice is not received for some reason, the user is taken back to the 
// start screen with a message that no invoice was received
void checkInvoiceReceived() {
#ifdef DEBUG
  Serial.println("[checkInvoiceReceived]");
#endif  
    internal_set_message("NO INVOICE RECEIVED",3);
    ui_load_about_screen();
}




void ws_lnurlw_withdraw(const char *lnurlw) {
    Serial.println("[ws_lnurlw_withdraw]");
    internal_set_message("PROCESSING NFC PAYMENT",15);
    String wsmessage = PSTR("{\"event\":\"lnurlw\",\"payment_request\":\""); 
    wsmessage += payment_request;
    wsmessage += PSTR("\",\"lnurlw\":\"");
    wsmessage += lnurlw;
    wsmessage += PSTR("\"}");
    webSocket.sendTXT(wsmessage);  
}

// firmware update
bool main_firmware_update() {
    Serial.println("[main_firmware_update]");

    if ( xSemaphoreTake(control_sem,pdMS_TO_TICKS(1000)) == pdTRUE ) {
        Serial.println("[main_firmware_update] setting bDoFirmwareUpdate to true");
        bDoFirmwareUpdate = true;
        firmwareUpdateTask.restartDelayed(2000);
        xSemaphoreGive(control_sem);        
        return true;
    }  else {
        Serial.println("[main_firmware_update] failed to obtain semaphore");
        return false;
    }
}

void firmwareUpdateFinished() {
    ui_set_payment_message("RESTARTING",20000);
}

void firmwareUpdateProgress(int cur, int total) {
  static char message[25];
  snprintf_P(message, sizeof(message), PSTR("UPDATE PROGRESS %d%%"), 100 * cur / total);
  ui_set_payment_message(message,10000);
  Serial.println(message);
}

void firmwareUpdateError(int err) {
    Serial.printf("firmwareUpdateError %d\n",err);
    switch (err ) {
      case HTTP_UE_NO_PARTITION:
        ui_set_payment_message("NO PARTITION",10000);
        break;
      case HTTP_UE_BIN_FOR_WRONG_FLASH:
        ui_set_payment_message("WRONG FLASH",10000);
        break;
      case HTTP_UE_BIN_VERIFY_HEADER_FAILED:
        ui_set_payment_message("VERIFY FAILED",10000);
        break;
      case HTTP_UE_SERVER_FAULTY_MD5:
        ui_set_payment_message("HASH ERROR",10000);
        break;
      case HTTP_UE_SERVER_WRONG_HTTP_CODE:
        ui_set_payment_message("BAD SERVER RESPONSE",10000);      
        break;
      case HTTP_UE_SERVER_FORBIDDEN:
        ui_set_payment_message("NOT ALLOWED",10000);
        break;
      case HTTP_UE_SERVER_FILE_NOT_FOUND:
        ui_set_payment_message("FILE NOT FOUND",10000);
        break;
      case HTTP_UE_SERVER_NOT_REPORT_SIZE:
        ui_set_payment_message("SIZE NOT REPORTED",10000);
        break;
      case HTTP_UE_TOO_LESS_SPACE:
        ui_set_payment_message("NOT ENOUGH SPACE",10000);
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
    checkNetworkTask.disable();
    reportStateTask.disable();
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




// called from UI thread, so lock before accessing variables
bool main_verify_pin(String entered_pin) {
#ifdef DEBUG
    Serial.println("[main_verify_pin]");
#endif    
    String pin = "";
    
    if ( xSemaphoreTake(control_sem,pdMS_TO_TICKS(1000)) == pdTRUE ) {
        pin = main_shared_tapConfig.getPIN();
        xSemaphoreGive(control_sem);        
    } else {
        return false;
    }

    if (( pin.length() == 6 )&&(entered_pin.length() == 6)&&(pin.equals(entered_pin))) {
        return true;
    } else {
        return false;
    }

}

// called from UI thread, so lock before accessing variables
void main_get_invoice(int product) {
    int i;

    if ( xSemaphoreTake(control_sem,portMAX_DELAY) == pdTRUE ) {


        ws_request_invoice(productConfig.getProduct(product)->getSwitchID());
        xSemaphoreGive(control_sem);        
    }
}

// called from UI thread, so lock before accessing variables
TapConfig main_get_tapconfig() {
    TapConfig temp;
    if ( xSemaphoreTake(control_sem,portMAX_DELAY) == pdTRUE ) {
        temp = TapConfig(&main_shared_tapConfig);
        xSemaphoreGive(control_sem);        
    }
    return temp;
}

// called from UI thread, so lock before accessing variables
void main_update_tapconfig(TapConfig cfg) {
    if ( xSemaphoreTake(control_sem,portMAX_DELAY) == pdTRUE ) {
        main_shared_tapConfig = TapConfig(&cfg);
        main_shared_tapConfig.save();
        webSocket.disconnect();
        WiFi.disconnect();
        xSemaphoreGive(control_sem);        
    }    
}

// called when something succesfull was read from the NFC sensor
void main_nfc_read_success(const uint8_t *buffer,int num) {
#ifdef DEBUG
    Serial.println("[main_nfc_read_succes]");
#endif    
    if ( xSemaphoreTake(control_sem,portMAX_DELAY) == pdTRUE ) {
        internal_set_message("NFC READ",4);

        // if there is an invoice availble, do the withdraw now, otherwise, requiest an invoice
        if ( payment_request.length() > 0 ) {           
            // there is a payment request, do the withdraw now                         
            ws_lnurlw_withdraw((const char *)buffer);             
        } else {
            // there is no payment request, copy the lnurlw
            memcpy(main_shared_lnurlw,buffer,num);
            main_shared_lnurlw_len = num;

            // and request an invoice
            ws_request_invoice(productConfig.getProduct(0)->getSwitchID());
        } 
        
        xSemaphoreGive(control_sem);        
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

// handle a websocket event
// This function is called by the websocket.loop function, thus semaphore locking should not be done here
void ws_event(WStype_t type, uint8_t * payload, size_t length) {
#ifdef DEBUG
    Serial.println("[ws_event]");
#endif
    switch ( type ) {
        case WStype_DISCONNECTED:
           Serial.println("WebSocket Disconnected");
           break;
        case WStype_CONNECTED:
            Serial.println("WebSocket Connected");
            break;
        case WStype_TEXT:
            {
                DynamicJsonDocument doc(2000);
                DeserializationError error = deserializeJson(doc, (char *)payload);
                if ( error != DeserializationError::Ok ) {
                    Serial.println("Deserialisation error");
                    return;
                }
                const char *eventtype = doc["event"].as<const char *>();
                Serial.printf("[ws_event] eventtype: %s\n",eventtype);
                if ( strcmp(eventtype,WS_EVENTTYPE_SWITCHES) == 0 ) {
                    tone_switches();

                    productConfig.parse(&doc);
                    productConfig.save();

                    internal_set_message("CONNECTED",1);


                    // configure switches in the UI
                    const char *labels[PARTYTAP_CFG_MAX_PRODUCTS];
                    for (int i=0;i<PARTYTAP_CFG_MAX_PRODUCTS;i++) {
                        labels[i] = (char *)productConfig.getProduct(i)->getLabel();
                    }
                    ui_init_switches(productConfig.getNumProducts(),(const char **)labels);
                } else if ( strcmp(eventtype,WS_EVENTTYPE_INVOICE) == 0 ) {
                    payment_request = doc["pr"].as<const char *>();
                    payment_hash = doc["payment_hash"].as<const char *>();

                    // start an expire invoice task
                    checkInvoiceReceivedTask.disable();
                    expireInvoiceTask.restartDelayed(TASK_SECOND * 120);
                        
                    // make a withdraw is lengthis larger than 0, otherwise navigate to the payment screen
                    Serial.printf("[ws_event] main_shared_lnurlw_len = %d\n",main_shared_lnurlw_len);       
                    if ( main_shared_lnurlw_len > 0 ) {                 
                        ws_lnurlw_withdraw((const char *)main_shared_lnurlw); 
                        main_shared_lnurlw[0] = '\0';
                        main_shared_lnurlw_len = 0;                     
                    } else {
                        ui_hide_payment_message();
                    }
                    
                    ui_load_payment_screen();
                    ui_display_qrcode(payment_request.c_str(),payment_request.length());
                    
                } else if ( strcmp(eventtype,WS_EVENTTYPE_PAID) == 0 ) {
                    tone_paid();
                    internal_set_message("INVOICE PAID",3);


                    // expiore the invoice
                    expireInvoiceTask.disable();
  
  
                    // reset data
                    payment_pin = "";
                    payment_hash = "";
                    payment_request = "";
                    
                    int tap_credit = doc["payload"].as<int>();
                    ui_add_tap_credit(tap_credit);
                } else if ( strcmp(eventtype,WS_EVENTTYPE_PAYMENTFAILED) == 0 ) {   
                    internal_set_message("PAYMENT FAILED",5);
                    tone_paymentfailed();
                    Serial.println("[ws_event] payment Failed");
                   //tone_paymentfailed();
                   // delay(200);
                } else {
                    Serial.println("another");
                }
            }
            break;
        case WStype_PING:
            break;
        default:
            Serial.printf("[ws_event] unhandled event of type = %d\n",type);
            break;
    }
}


  
void nfc_loop(void *parameters) 
{ 
    int msg = 0;

    uint8_t nfcbuffer[512];
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; 
    uint8_t uidLength;
    uint8_t success = 0;
    

    while ( 1 == 1 ) {
        success = pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,60000);
    


        if ( ! success ) {
            Serial.println("no succes reading");
            continue;
        }

        ui_set_payment_message("READING NFC",5);

        if ((uidLength != 7) && (uidLength != 4)) {
            Serial.println("uidLength incorrect");
            ui_set_payment_message("NFC READ ERROR",3);
            continue;
        }   

        if (!pn532->ntag424_isNTAG424()) {
            ui_set_payment_message("NFC READ ERROR",3);
            Serial.println("No NTAG424");
            continue;
        }

        uint8_t bytesread = pn532->ntag424_ISOReadFile(nfcbuffer,sizeof(nfcbuffer));
        nfcbuffer[bytesread] = 0;
    
        if ( bytesread == 0 ) {
            Serial.println("no bytes read");
            ui_set_payment_message("NFC READ ERROR",3);

            continue;
        }



        if ( strncmp("lnurlw://",(char *)(nfcbuffer),9) != 0 ) {
            Serial.println("this is not an LNURLW");
            ui_set_payment_message("NFC READ ERROR",3);
            continue;
        }

        Serial.printf("Read succes: %s\n",nfcbuffer);


        main_nfc_read_success(nfcbuffer, bytesread); 
          
    } 
}


// expire the current available invoice
// do not lock the main_loop in this function
void expireInvoice() {
#ifdef DEBUG
    Serial.println("[expireInvoice]");
#endif
    // Only go back to the about screen, if we're on the payment page
    if ( ui_payment_screen_active() ) {
        Serial.println("[expireInvoice] TAP not in progress or payment screen is active") ;
        // navigate to the about screen
        ui_load_about_screen();
    } else {
        Serial.println("[expireInvoice] payment screen is not active");
    }

    // expire the task
    expireInvoiceTask.disable();
  
    // reset data
    payment_pin = "";
    payment_hash = "";
    payment_request = "";  
}


void main_loop(void *parameters) {    
#ifdef DEBUG
    Serial.println("[main_loop]");
#endif
    Scheduler taskScheduler;

    taskScheduler.addTask(checkNetworkTask);
    taskScheduler.addTask(reportStateTask);
    taskScheduler.addTask(expireInvoiceTask);
    taskScheduler.addTask(firmwareUpdateTask);

    reportStateTask.enable();


    webSocket.onEvent(ws_event);
            

    // start the network task
#ifdef DEBUG
    Serial.println("[main_loop] starting network task");
#endif
    checkNetworkTask.restart();

    // when websocket hangs, ie when lnbits is broken. it may appear that the program is frozen

    while ( 1 == 1 ) {  

        if ( xSemaphoreTake(control_sem,200) == pdTRUE ) {
            taskScheduler.execute();
            xSemaphoreGive(control_sem);
        }

        if ( xSemaphoreTake(control_sem,200) == pdTRUE ) {
            webSocket.loop();
            xSemaphoreGive(control_sem);
        }
            
        vTaskDelay(100);

    }
}

// Initialize the NFC sensor
void nfc_init() {
#ifdef DEBUG
    Serial.println("[nfc_init]");
#endif
    nfc_sem = xSemaphoreCreateMutex();

    wire = new TwoWire(1);
    wire->begin(17,18);
    pn532 = new Adafruit_PN532(-1,-1,wire);
    if ( pn532->begin() ) {
        Serial.println("Succes init PN532");
    } else {
        Serial.println("Fail init PN532");
        return;
    }

    uint32_t versiondata = pn532->getFirmwareVersion();
    if (!versiondata) {
        Serial.println("No version data");
        return;
    } else {
        Serial.println("Got version data");
        Serial.println(versiondata);
    }


    if ( ! pn532->SAMConfig() ) {
        Serial.println("[nfc_init] SAMConfig fail");
        return;
    } else {
        Serial.println("[nfc_init] SAMConfig succes");
    }
        
#ifdef DEBUG
    Serial.println("[nfc_init] Starting NFC task");
#endif    
    if ( xTaskCreatePinnedToCore (
            nfc_loop,     // Function to implement the task
            "loopNFC",   // Name of the task
            4096,      // Stack size in bytes
            NULL,      // Task input parameter
            10,         // Priority of the task
            &nfcTask_Handle,      // Task handle.
            0          // Core where the task should run
        ) == pdTRUE ) {
            Serial.println("[nfc_init] task successfully started");
        } else {
            Serial.println("[nfc_init] failed to start NFC task");
        }


}


// Arduino initial setup call
void setup() {
    delay(5000);

    BMP.begin();
    

    tone_online();



    Serial.begin(115200);
    Serial.println("[setup] Starting up");

    LittleFS.begin(true);
    main_shared_tapConfig.load();

    //TODO: Share tap config with UI thread
    
    control_sem = xSemaphoreCreateMutex();

    
    // Initialise UI
    ui_init();

    // Start UI thread
   if ( xTaskCreatePinnedToCore (
        ui_loop,     // Function to implement the task
        "ui_loop",   // Name of the task
        10000,       //  Stack size in bytes
        NULL,      // Task input parameter
        5,         // Priority of the task
        &lvglTask_Handle,      // Task handle.
        1          // Core where the task should run
      ) != pdPASS ) {
        Serial.println("Failed to start UI thread");
      }


    nfc_init();

    if ( xTaskCreatePinnedToCore (
        main_loop,     // Function to implement the task
        "main_loop",   // Name of the task
        10000,      // Stack size in bytes
        NULL,      // Task input parameter
        10,         // Priority of the task
        &controlTask_Handle,      // Task handle.
        0          // Core where the task should ru
     ) ) {
        Serial.println("[setup] main_loop started");
    } else {
        Serial.println("[setup] failed to start mainloop");
    }



    // UBaseType_t iFreeStackNum = 0;
    // while ( 1 == 1 ) {
    //     Serial.printf("Number of tasks: %d\n",uxTaskGetNumberOfTasks());
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(nfcTask_Handle);
    //     Serial.printf("NFC Stack Free: %d\n",iFreeStackNum);
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(lvglTask_Handle);
    //     Serial.printf("LVGL Stack Free: %d\n",iFreeStackNum);
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(controlTask_Handle);
    //     Serial.printf("Control Stack Free: %d\n",iFreeStackNum);
    //     vTaskDelay(30000/portTICK_PERIOD_MS);
    // }    
}


// report state to the GUI process
void reportState() {    
    if ( ! ui_admin_screen_active() ) {
        return;
    }
#ifdef DEBUG
    Serial.println("[reportState]");
#endif


    int wifiState = WiFi.status();
    switch ( wifiState ) {
        case WL_CONNECTED:
            ui_update_wifi_state("Connected");
            break;
        case WL_DISCONNECTED:
            ui_update_wifi_state("Disconnected");
            break;
        case WL_IDLE_STATUS:
            ui_update_wifi_state("Idle");
            break;
        case WL_STOPPED:
            ui_update_wifi_state("Stopped");
            break;
        case WL_NO_SSID_AVAIL:
            ui_update_wifi_state("SSID Unavailable");
            break;
        case WL_CONNECT_FAILED:
            ui_update_wifi_state("Connection failed");
            break;
        case WL_CONNECTION_LOST:
            ui_update_wifi_state("Connection lost");
            break;
    }
    if ( webSocket.isConnected() ) {
        ui_update_websocket_state("Connected");
    } else {
        ui_update_websocket_state("Disconnected");
    }
    ui_update_server_version(productConfig.getServerVersion());
    ui_update_server_branding(productConfig.getServerBranding());
}

// check the network and restart if needed
// this task is executed by the task scheduler, which is already locked
void checkNetwork()
{
#ifdef DEBUG
    Serial.println("[checkNetwork]");
#endif
    switch ( WiFi.status() ) {
        case WL_IDLE_STATUS:
            internal_set_message("WIFI NOT CONNECTED",5);        
        case WL_STOPPED:
#ifdef DEBUG
            Serial.println("[checkNetwork] Starting WiFi");
#endif
            WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
            WiFi.begin(main_shared_tapConfig.getWiFiSSID(),main_shared_tapConfig.getWiFiPWD());
            break;
        case  WL_CONNECTED:
            Serial.println("WL_CONNECTED");
            if ( ! webSocket.isConnected() ) {
                Serial.println("WEBSOCKET NOT CONNECTED");

                internal_set_message("NOT CONNECTED",3);
                ui_hide_payment_buttons();

                config_wspath = "/partytap/api/v1/ws/";
                config_wspath += main_shared_tapConfig.getDeviceID();                
#ifdef DEBUG
                Serial.printf("[checkNetwork] Starting WebSocket to: %s\n",config_wspath.c_str());
#endif
                webSocket.disconnect();
                webSocket.enableHeartbeat(5000,2000,1);
                webSocket.beginSSL(main_shared_tapConfig.getLNbitsHost(),443, config_wspath);                
            } else {
                Serial.println("WEBSOCKET CONNECTED");
            }
            break;
        case WL_DISCONNECTED:
#ifdef DEBUG
            Serial.println("[checkNetwork] WL_DISCONNECTED");
#endif
            WiFi.begin(main_shared_tapConfig.getWiFiSSID(),main_shared_tapConfig.getWiFiPWD());            
            break;
            
        default:
            Serial.printf("WIFI STATUS: %d\n",WiFi.status());

            break;
    }

}


void loop() {      
}







