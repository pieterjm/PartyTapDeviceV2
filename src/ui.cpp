#include <lvgl.h>
#include <Arduino.h>
#include "pincfg.h"
#include "dispcfg.h"
#include "AXS15231B_touch.h"
#include <Arduino_GFX_Library.h>
#include "ui.h"
#include "bitcointap.h"
#include "network.h"
#include <ESP32Servo.h>
#include <WiFiType.h>

TaskHandle_t uiTaskHandle;
QueueHandle_t uiQueue = NULL;
TimerHandle_t hideMessageTimer;
TimerHandle_t lvglTimer;
TimerHandle_t progressTimer;
TimerHandle_t backToAboutTimer;
TimerHandle_t tapTimeoutTimer;

#define TASK_NOTIFICATION_PULSE_MASK (0x0000FFFF)
#define TASK_NOTIFICATION_LVGL (1 << 17)
#define TASK_NOTIFICATION_PROGRESS (1 << 18)

#define LANDSCAPE
#ifdef PORTRAIT
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#elifdef LANDSCAPE
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#endif

struct uiQueueMessage
{
    UIQueueMessageType type;
    uint16_t data;
};

String ui_PinEnteredPin = ""; // The actual entered PIN on the PIN Screen
String ui_PinHidePin = "";    // a string of stars displayed on the screen
TapConfig ui_tapConfig;
bool bFreeDrink = false;
int ui_tap_calibration_ticks = 0; // amount of ticks or time passed
bool bAdminMode = false;
Servo *ui_servo;
bool ui_tap_running = false;
long ui_tap_remaining_volume = 0;
int ui_selected_product = 0;

SemaphoreHandle_t uiPaymentDataSemaphore;
String uiPaymentRequest = "";
String uiPaymentCurrency = "";
float uiPaymentAmount = 0;

#define UI_BUTTON_TAP_TAP "START"
#define UI_BUTTON_TAP_PAUSE "PAUSE"
#define UI_BUTTON_TAP_RESUME "RESUME"
#define UI_LABEL_TAP_FINISHED "ENJOY YOUR DRINK!"
#define UI_LABEL_TAP_INSTRUCTION "HOLD YOUR GLASS UNDER\nTHE TAP AND PRESS START"
#define UI_TAP_PROGRESS_INTERVAL 200

//
Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, TFT_res_W, TFT_res_H);
Arduino_Canvas *gfx = new Arduino_Canvas(TFT_res_W, TFT_res_H, g, 0, 0, TFT_rot);
AXS15231B_Touch touch(Touch_SCL, Touch_SDA, Touch_INT, Touch_ADDR, TFT_rot);

// Declare the main font
LV_FONT_DECLARE(ui_font_UbuntuBoldItalic);

// the tap screen
lv_obj_t *ui_ScreenTap;
lv_obj_t *ui_ImageTap;
lv_obj_t *ui_LabelTapInstructions;
lv_obj_t *ui_LabelTapStart;
lv_obj_t *ui_BarTapProgress;
lv_obj_t *ui_ButtonTapStart;

// the admin screen
lv_obj_t *ui_ScreenAdmin;
// lv_obj_t *ui_PanelAdminHeader;
// lv_obj_t *ui_LabelAdminHeader;
// lv_obj_t *ui_LabelAdminVersion;
// lv_obj_t *ui_LabelAdminServerVersion;
// lv_obj_t *ui_LabelAdminBranding;
lv_obj_t *ui_LabelAdminWiFiStatus;
lv_obj_t *ui_LabelAdminWebSocketStatus;
lv_obj_t *ui_ButtonAdminCalibrate;
lv_obj_t *ui_LabelAdminCalibrate;
lv_obj_t *ui_LabelAdminWiFiStrength;
lv_obj_t *ui_ButtonAdminConfig;
lv_obj_t *ui_LabelAdminConfig;
lv_obj_t *ui_ButtonAdminFree;
lv_obj_t *ui_LabelAdminFree;
lv_obj_t *ui_ButtonAdminCancel;
lv_obj_t *ui_LabelAdminCancel;
lv_obj_t *ui_ButtonAdminOpen;
lv_obj_t *ui_LabelAdminOpen;
lv_obj_t *ui_ButtonAdminClose;
lv_obj_t *ui_LabelAdminClose;
// lv_obj_t *ui_ButtonAdminCalibrate;
// lv_obj_t *ui_LabelAdminCalibrate;
lv_obj_t *ui_LabelAdminCalibrationTicks;

// the enter pin screen
lv_obj_t *ui_ScreenPin;
lv_obj_t *ui_LabelPinMessage;
lv_obj_t *ui_ButtonPin0;
lv_obj_t *ui_LabelPin0;
lv_obj_t *ui_ButtonPin1;
lv_obj_t *ui_LabelPin1;
lv_obj_t *ui_ButtonPin2;
lv_obj_t *ui_LabelPin2;
lv_obj_t *ui_ButtonPin3;
lv_obj_t *ui_LabelPin3;
lv_obj_t *ui_ButtonPin4;
lv_obj_t *ui_LabelPin4;
lv_obj_t *ui_ButtonPin5;
lv_obj_t *ui_LabelPin5;
lv_obj_t *ui_ButtonPin6;
lv_obj_t *ui_LabelPin6;
lv_obj_t *ui_ButtonPin7;
lv_obj_t *ui_LabelPin7;
lv_obj_t *ui_ButtonPin8;
lv_obj_t *ui_LabelPin8;
lv_obj_t *ui_ButtonPin9;
lv_obj_t *ui_LabelPin9;
lv_obj_t *ui_ButtonPinCancel;
lv_obj_t *ui_LabelPINCancel;
lv_obj_t *ui_ButtonPinOK;
lv_obj_t *ui_LabelPinOK;

// the about screen
lv_obj_t *ui_ScreenAbout;
// lv_obj_t *ui_PanelAboutHeader;
// lv_obj_t *ui_LabelAboutHeader;
lv_obj_t *ui_ImageAbout;
lv_obj_t *ui_LabelAboutMessage;
lv_obj_t *ui_ButtonAboutOne;
lv_obj_t *ui_ButtonAboutTwo;
lv_obj_t *ui_ButtonAboutThree;
lv_obj_t *ui_LabelAboutOne;
lv_obj_t *ui_LabelAboutTwo;
lv_obj_t *ui_LabelAboutThree;
LV_IMG_DECLARE(ui_img_bitcointaps256);

// the payment screen
lv_obj_t *ui_ScreenPayment;
lv_obj_t *ui_LabelPaymentHeader;
lv_obj_t *ui_ButtonPaymentCancel;
lv_obj_t *ui_LabelPaymentCancel;
lv_obj_t *ui_ButtonPaymentEnterPin;
lv_obj_t *ui_LabelPaymentEnterPin;
lv_obj_t *ui_QrcodePayment;
lv_obj_t *ui_PanelPaymentMessage;
lv_obj_t *ui_LabelPaymentMessage;

// The configuration screen
lv_obj_t *ui_ScreenConfig;
lv_obj_t *ui_LabelConfigSSID;
lv_obj_t *ui_TextAreaConfigSSID;
lv_obj_t *ui_LabelConfigWifiPassword;
lv_obj_t *ui_TextAreaConfigWifiPassword;
lv_obj_t *ui_LabelConfigLNbitsHost;
lv_obj_t *ui_TextAreaConfigLNbitsHost;
lv_obj_t *ui_LabelConfigDeviceID;
lv_obj_t *ui_TextAreaConfigDeviceID;
lv_obj_t *ui_LabelConfigServo;
lv_obj_t *ui_TextAreaConfigServoClosed;
lv_obj_t *ui_TextAreaConfigServoOpen;
lv_obj_t *ui_ButtonPIN;
lv_obj_t *ui_ButtonConfigServoClose;
lv_obj_t *ui_LabelConfigServoClose;
lv_obj_t *ui_LabelConfigPaymentMode;
lv_obj_t *ui_DropdownConfigPaymentMode;
lv_obj_t *ui_ButtonConfigServoOpen;
lv_obj_t *ui_LabelConfigControlMode;
lv_obj_t *ui_DropdownConfigControlMode;
lv_obj_t *ui_LabelConfigVolumePerUnit;
lv_obj_t *ui_TextAreaConfigVolumePerUnit;
lv_obj_t *ui_ButtonConfigSave;
lv_obj_t *ui_LabelConfigServoOpen;
lv_obj_t *ui_LabelConfigDone;
lv_obj_t *ui_ButtonConfigCancel;
lv_obj_t *ui_LabelConfigCancel;
lv_obj_t *ui_LabelConfigUpdate;
lv_obj_t *ui_ButtonConfigUpdate;
lv_obj_t *ui_PanelConfigPIN;
lv_obj_t *ui_LabelConfigPIN;
lv_obj_t *ui_LabelConfigPINCurrent;
lv_obj_t *ui_TextAreaConfigPINCurrent;
lv_obj_t *ui_LabelConfigPINNew;
lv_obj_t *ui_TextAreaConfigPINNew;
lv_obj_t *ui_LabelConfigPINRepeat;
lv_obj_t *ui_TextAreaConfigPINRepeat;
lv_obj_t *ui_ButtonConfigPINOK;
lv_obj_t *ui_LabelConfigButtonPINOK;
lv_obj_t *ui_ButtonConfigPINCancel;
lv_obj_t *ui_LabelConfigButtonPINCancel;
lv_obj_t *ui_LabelConfigPINMessage;
lv_obj_t *ui_KeyboardConfig;


#define COLOR_BACKGROUND  0x000000
#define COLOR_BORDER      0xFFFFFF
#define COLOR_TEXT        0xFFFFFF

#define COLOR_BUTTON       0xF7931A
#define COLOR_BUTTON_TEXT  0x000000

#define COLOR_HEADER       0xF7931A
#define COLOR_HEADER_TEXT  0xFFFFFF


#ifdef BRANDING_WHY2025
#define BB_NAME "Why Drink? Why Not!"
#else
#define BB_NAME "BitcoinTaps"
#endif
#define FIRMWARE_VERSION "test"

void internal_servo_write(int i)
{
    ui_servo->write(i);
}

void internal_tap_open()
{
    internal_servo_write(ui_tapConfig.getServoOpen());
    digitalWrite(RELAY_PIN, HIGH);
    ui_tap_running = true;
}

void internal_tap_close()
{
    internal_servo_write(ui_tapConfig.getServoClose());
    digitalWrite(RELAY_PIN, LOW);
    ui_tap_running = false;
}

// close the tap
// called from UI thread, so lock before accessing variables
void internal_tap_pause()
{
    internal_tap_close();
}

// open the tap
// called from UI thread, so lock before accessing variables
void internal_tap_resume()
{
    internal_tap_open();
}

void ui_load_screen(lv_obj_t *screen) {    
    if ( screen == ui_ScreenAdmin ) {
        ui_tap_calibration_ticks = 0;
        bAdminMode = true;
        xTimerStart(networkStateReportTimer,portMAX_DELAY);
    } else if ( screen == ui_ScreenAbout ) {
        lv_label_set_text(ui_LabelAboutMessage,"PRESS BUTTON TO START");
    } else {
        bAdminMode = false;
        xTimerStop(networkStateReportTimer,portMAX_DELAY);
    }
    lv_disp_load_scr(screen);
}


void ui_init_tap_screen()  {
    lv_label_set_text(ui_LabelTapInstructions, UI_LABEL_TAP_INSTRUCTION);
    lv_obj_clear_flag(ui_BarTapProgress, LV_OBJ_FLAG_HIDDEN);
    if ( ui_tapConfig.getControlMode() == CONTROL_MODE_TIME ) {
        lv_label_set_text(ui_LabelTapStart, UI_BUTTON_TAP_TAP);
        lv_obj_clear_flag(ui_ButtonTapStart, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_ButtonTapStart, LV_OBJ_FLAG_HIDDEN);
    }
}

// Tap progress
// running in the main loop process, do not lock here
void update_tapprogress()
{
    static char ui_text[40];

    // do nothing if the tap is not running
    if ( ! ui_tap_running ) {
        return;
    }

  
    // send ticks event to count time if in time mode
    if ( ui_tapConfig.getControlMode() == CONTROL_MODE_TIME ) {
        xTaskNotify(uiTaskHandle, UI_TAP_PROGRESS_INTERVAL,eIncrement);
    }

    // update the UI
    if ( ui_ScreenTap == lv_screen_active() ) {
        lv_bar_set_value(ui_BarTapProgress, ui_tap_remaining_volume, LV_ANIM_OFF);    
    } else if ( ui_ScreenAdmin == lv_screen_active() ) {
        snprintf_P(ui_text, sizeof(ui_text), PSTR("Calibration ticks: %d"), ui_tap_calibration_ticks);
        lv_label_set_text(ui_LabelAdminCalibrationTicks,ui_text);
    }
}

// called from UI thread, so lock before accessing variables
bool internal_verify_pin(String entered_pin)
{
    String pin = ui_tapConfig.getPIN();

    if ((pin.length() == 6) && (entered_pin.length() == 6) && (pin.equals(entered_pin)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool ui_add_tap_volume(int volume)
{
    debug("ui_add_tap_volume","Enter function");
    ui_tap_remaining_volume += volume;
    // lv_bar_set_range(ui_BarTapProgress, 0, ui_tap_remaining_volume);
    lv_bar_set_range(ui_BarTapProgress, 0, 1000);
    return true;
}

// update the state of the websocket in the admin screen
// OBSOLETE
// bool ui_update_websocket_state(const char *s)
// {
//     lv_label_set_text_fmt(ui_LabelAdminWebSocketStatus, "WebSocket Status: %s", s);
//     return true;
// }

// OBSOLETE
// update the status string of the wifi connection in the admin screeen
// bool ui_update_wifi_state(const char *s)
// {
//     lv_label_set_text_fmt(ui_LabelAdminWiFiStatus, "WiFi Status: %s", s);
//     return true;
// }

// update the server version string in the admin screen
// bool ui_update_server_version(const char *s)
// {
//     // lv_label_set_text_fmt(ui_LabelAdminServerVersion, "Server version: %s", s);
//     return true;
// }

// update the server branding in the admin screen
// bool ui_update_server_branding(const char *s)
// {
//     // lv_label_set_text_fmt(ui_LabelAdminBranding, "Server branding: %s", s);
//     return true;
// }

// Set the text on the admin screen for the wifi status
void admin_report_wifi_state(wl_status_t status) {
    switch ( status ) {
        case WL_IDLE_STATUS:
            lv_label_set_text(ui_LabelAdminWiFiStatus,"WiFi Status: Not connected");
            break;
        case WL_NO_SSID_AVAIL:
            lv_label_set_text(ui_LabelAdminWiFiStatus,"WiFi Status: SSID not found");
            break;
        case WL_CONNECTED:
            lv_label_set_text(ui_LabelAdminWiFiStatus,"WiFi Status: Connected");
            break;
        case WL_STOPPED:
            lv_label_set_text(ui_LabelAdminWiFiStatus,"WiFi Status: Stopped");
            break;
        case WL_CONNECTION_LOST:
        case WL_CONNECT_FAILED:
        default:
            lv_label_set_text_fmt(ui_LabelAdminWiFiStatus,"WiFi Status: Unknown (%d)",status);
            break;
    }
}

// set the text on the admin screen for the websocket status
void admin_report_websocket_state(NetworkWebSocketState state) {
    switch ( state ) {
        case WEBSOCKET_NOT_CONNECTED:
            lv_label_set_text(ui_LabelAdminWebSocketStatus,"WebSocket: Not connected");
            break;
        case WEBSOCKET_CONNECTED:
            lv_label_set_text(ui_LabelAdminWebSocketStatus,"WebSocket: Connected");
            break;
        case WEBSOCKET_INCORRECT_DEVICEID:
            lv_label_set_text(ui_LabelAdminWebSocketStatus,"WebSocket: Device ID incorrect");
            break;
        case WEBSOCKET_UNKNOWN_ERROR:
            lv_label_set_text(ui_LabelAdminWebSocketStatus,"WebSocket: Unknown error");
            break;
        default:
            lv_label_set_text(ui_LabelAdminWebSocketStatus,"WebSocket: Unknown state");
            break;
    }
}

// set a popup message
void set_message(uiQueueMessage msg)
{
    String message = "";
    switch (msg.type) {
    case SHOW_MESSAGE_BAD_SERVER_RESPONSE:
        message = "BAD SERVER RESPONSE";
        break;
    case SHOW_MESSAGE_CREATING_INVOICE:
        message = "CREATING INVOICE";
        break;
    case SHOW_MESSAGE_FILE_NOT_FOUND:
        message = "FILE NOT FOUND";
        break;
    case SHOW_MESSAGE_HASH_ERROR:
        message = "HASH ERROR";
        break;
    case SHOW_MESSAGE_INVALID_CARD:
        message = "INVALID CARD";
        break;
    case SHOW_MESSAGE_INVOICE_PAID:
        message = "INVOICE PAID";
        break;
    case SHOW_MESSAGE_NFC_READ_ERROR:
        message = "NFC READ ERROR";
        break;
    case SHOW_MESSAGE_NFC_READ_SUCCES:
        message = "NFC READ SUCCES";
        break;
    case SHOW_MESSAGE_NO_INVOICE_RECEIVED:
        message = "NO INVOICE RECEIVED";
        break;
    case SHOW_MESSAGE_NO_PARTITION:
        message = "NO PARTITION";
        break;
    case SHOW_MESSAGE_NOT_ALLOWED:
        message = "NOT ALLOWED";
        break;
    case SHOW_MESSAGE_NOT_CONNECTED:
        message = "NOT CONNECTED";
        break;
    case SHOW_MESSAGE_NOT_ENOUGH_SPACE:
        message = "NOT ENOUGH SPACE";
        break;
    case SHOW_MESSAGE_PAYMENT_FAILED:
        message = "PAYMENT FAILED";
        break;
    case SHOW_MESSAGE_PROCESSING_MFC_PAYMENT:
        message = "PROCESSING NFC PAYMENT";
        break;
    case SHOW_MESSAGE_READING_NFC:
        message = "READING NFC";
        break;
    case SHOW_MESSAGE_RESTARTING:
        message = "RESTARTING";
        break;
    case SHOW_MESSAGE_SIZE_NOT_REPORTED:
        message = "SIZE NOT REPORTED";
        break;
    case SHOW_MESSAGE_UPDATE_PROGRESS:
        // set_text_fmt is used below to format this message
        break;
    case SHOW_MESSAGE_VERIFY_FAILED:
        message = "VERIFY FAILED";
        break;
    case SHOW_MESSAGE_WEBSOCKET_NOT_CONNECTED:
        message = "WEBSOCKET NOT CONNECTED";
        break;
    case SHOW_MESSAGE_WIFI_NOT_CONNECTED:
        message = "WIFI NOT CONNECTED";
        break;
    case SHOW_MESSAGE_WRONG_FLASH:
        message = "WRONG FLASH";
        break;
    case SHOW_MESSAGE_SSID_NOT_FOUND:
        message = "SSID NOT FOUND";
        break;
    default:
        message = "UNDEFINED";
        break;
    }


    if ( msg.type == SHOW_MESSAGE_UPDATE_PROGRESS ) {
        lv_label_set_text_fmt(ui_LabelAboutMessage, "UPDATE PROGRESS %d %%",msg.data);
        lv_label_set_text_fmt(ui_LabelPaymentMessage, "UPDATE PROGRESS %d %%",msg.data);
    } else {
        debug("set_message",message.c_str());
        lv_label_set_text(ui_LabelAboutMessage, message.c_str());
        lv_label_set_text(ui_LabelPaymentMessage, message.c_str());
    }
    
    lv_obj_clear_flag(ui_PanelPaymentMessage, LV_OBJ_FLAG_HIDDEN);

    if (msg.data > 0)
    {
        xTimerChangePeriod(hideMessageTimer, pdMS_TO_TICKS(msg.data),1000);  // this should also start the timer
        xTimerReset(hideMessageTimer,1000);
    }
}

// display the qr code on the payment screen
bool ui_init_and_load_payment_screen(String pr, String currency, float amount)
{
    if ( xSemaphoreTake(uiPaymentDataSemaphore, portMAX_DELAY)) {
        // copy payment data to shared variables and release lock
        uiPaymentRequest = pr;
        uiPaymentCurrency =  currency;
        uiPaymentAmount = amount;
        xSemaphoreGive(uiPaymentDataSemaphore);

        // send event to the UI threat to display the payment screen
        ui_queue_send_message(INIT_AND_LOAD_PAYMENT_SCREEN,0);
        
        return true;
    }
    return false;
}

void init_and_load_payment_screen() {
    static char ui_text[30];
    if ( xSemaphoreTake(uiPaymentDataSemaphore, portMAX_DELAY)) {
        // prepare the payment screen
        lv_qrcode_update(ui_QrcodePayment, uiPaymentRequest.c_str(), uiPaymentRequest.length());
        if ( uiPaymentCurrency.equalsIgnoreCase("sat") ) {
            snprintf(ui_text,30,"PAY %.0f %s",uiPaymentAmount,uiPaymentCurrency.c_str());
        } else {
            snprintf(ui_text,30,"PAY %.2f %s",uiPaymentAmount,uiPaymentCurrency.c_str());
        }
        lv_label_set_text(ui_LabelPaymentHeader, ui_text);

        // release the lock
        xSemaphoreGive(uiPaymentDataSemaphore);
        
        // display the payment screen
        lv_obj_clear_flag(ui_QrcodePayment, LV_OBJ_FLAG_HIDDEN);        
        ui_load_screen(ui_ScreenPayment);
    }
}

// Hide the product selection buttons on the about screen
void hide_payment_buttons()
{
    lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_LabelPaymentMessage,"");
}

// send a message to the UI queue
void ui_queue_send_message(UIQueueMessageType type, uint16_t data)
{
    struct uiQueueMessage msg;
    msg.type = type;
    msg.data = data;
    if ( uiQueue != NULL )  {
        xQueueSend(uiQueue, &msg, portMAX_DELAY);
    } else {
        debug("ui_queue_send_message","no uiQueue defined");
    }
}

// show the product selection buttons
void show_payment_buttons()
{
    if (xSemaphoreTake(productConfigSemaphore, portMAX_DELAY) == pdTRUE)
    {
        switch (productConfig.getNumProducts())
        {
        case 1:
            lv_obj_clear_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui_LabelAboutOne, productConfig.getProduct(0)->getLabel());
            lv_obj_set_width(ui_ButtonAboutOne, SCREEN_WIDTH);
            lv_obj_set_x(ui_ButtonAboutOne, 0);
            lv_label_set_text(ui_LabelAboutMessage,"PRESS BUTTON TO START");
            break;
        case 2:
            lv_obj_clear_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui_LabelAboutOne, productConfig.getProduct(0)->getLabel());
            lv_label_set_text(ui_LabelAboutTwo, productConfig.getProduct(1)->getLabel());
            lv_obj_set_x(ui_ButtonAboutOne, -1 * ( SCREEN_WIDTH / 4 + 5));
            lv_obj_set_x(ui_ButtonAboutTwo, SCREEN_WIDTH / 4 + 5);
            lv_obj_set_width(ui_ButtonAboutOne, SCREEN_WIDTH / 2 - 10);
            lv_obj_set_width(ui_ButtonAboutTwo, SCREEN_WIDTH / 2 - 10);
            lv_label_set_text(ui_LabelAboutMessage,"PRESS BUTTON TO START");
            break;
        case 3:
            lv_obj_clear_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui_LabelAboutOne, productConfig.getProduct(0)->getLabel());
            lv_label_set_text(ui_LabelAboutTwo, productConfig.getProduct(1)->getLabel());
            lv_label_set_text(ui_LabelAboutThree, productConfig.getProduct(2)->getLabel());
            lv_obj_set_x(ui_ButtonAboutOne, -1 * ((SCREEN_WIDTH - 20) / 3 + 5));
            lv_obj_set_x(ui_ButtonAboutTwo, 0);
            lv_obj_set_x(ui_ButtonAboutThree, (SCREEN_WIDTH - 20) / 3 + 5);
            lv_obj_set_width(ui_ButtonAboutOne, (SCREEN_WIDTH - 20)/ 3);
            lv_obj_set_width(ui_ButtonAboutTwo, (SCREEN_WIDTH - 20)/ 3);
            lv_obj_set_width(ui_ButtonAboutThree, (SCREEN_WIDTH - 20)/ 3);
            lv_label_set_text(ui_LabelAboutMessage,"PRESS BUTTON TO START");
            break;
        default:
            lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui_LabelAboutMessage,"");
            break;
        }
        xSemaphoreGive(productConfigSemaphore);
    }
}

// Close button on the config screen clicked
void ui_event_button_config_servo_close_clicked(lv_event_t *e)
{
    int value = atoi(lv_textarea_get_text(ui_TextAreaConfigServoClosed));
    internal_servo_write(value);
}

// Open button on the config screen clicked
void ui_event_button_config_servo_open_clicked(lv_event_t *e)
{
    int value = atoi(lv_textarea_get_text(ui_TextAreaConfigServoOpen));
    internal_servo_write(value);
}

// update button clicked
void ui_event_button_config_update_clicked(lv_event_t *e)
{
    ui_load_screen(ui_ScreenAbout);

    lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_add_flag(ui_PanelAboutHeader, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_LabelAboutMessage, "UPDATING FIRMWARE");

    main_firmware_update();
}

// Button event to cancel config without saving changes
void ui_event_button_config_cancel_clicked(lv_event_t *e)
{
    // make call to main to start updating
    ui_load_screen(ui_ScreenAdmin);
}

// Button event to cancel config without saving changes
void ui_event_button_config_save_clicked(lv_event_t *e)
{
    char buf[20];

    ui_tapConfig.setWiFiSSID(lv_textarea_get_text(ui_TextAreaConfigSSID));
    ui_tapConfig.setWiFiPWD(lv_textarea_get_text(ui_TextAreaConfigWifiPassword));
    ui_tapConfig.setLNbitsHost(lv_textarea_get_text(ui_TextAreaConfigLNbitsHost));
    ui_tapConfig.setDeviceID(lv_textarea_get_text(ui_TextAreaConfigDeviceID));
    ui_tapConfig.setServoOpen(atoi(lv_textarea_get_text(ui_TextAreaConfigServoOpen)));
    ui_tapConfig.setServoClose(atoi(lv_textarea_get_text(ui_TextAreaConfigServoClosed)));
    ui_tapConfig.setVolumePerUnit(atoi(lv_textarea_get_text(ui_TextAreaConfigVolumePerUnit)));

    lv_dropdown_get_selected_str(ui_DropdownConfigPaymentMode, buf, sizeof(buf));
    if (strncasecmp(buf, "wi-fi", 5) == 0)
    {
        ui_tapConfig.setPaymentMode(PAYMENT_MODE_WIFI);
    }
    else if (strncasecmp(buf, "offline", 7) == 0)
    {
        ui_tapConfig.setPaymentMode(PAYMENT_MODE_OFFLINE);
    }
    else if (strncasecmp(buf, "auto", 4) == 0)
    {
        ui_tapConfig.setPaymentMode(PAYMENT_MODE_AUTO);
    }
    else if (strncasecmp(buf, "serial", 6) == 0)
    {
        ui_tapConfig.setPaymentMode(PAYMENT_MODE_SERIAL);
    }

    lv_dropdown_get_selected_str(ui_DropdownConfigControlMode, buf, sizeof(buf));
    if (strncasecmp(buf, "Time", 4) == 0)
    {
        ui_tapConfig.setControlMode(CONTROL_MODE_TIME);
    }
    else if (strncasecmp(buf, "Pulse", 5) == 0)
    {
        ui_tapConfig.setControlMode(CONTROL_MODE_PULSE);
    }
    else
    {
        ui_tapConfig.setControlMode(CONTROL_MODE_NONE);
    }

    ui_tapConfig.save();

    // copy updated config to main
    // update and restart network
    network_connect(ui_tapConfig);
    
    // go back to the admin screen
    ui_load_screen(ui_ScreenAdmin);
}

// pPIN button on the config clicked
void ui_event_button_config_pin_clicked(lv_event_t *)
{
    lv_textarea_set_text(ui_TextAreaConfigPINCurrent, "");
    lv_textarea_set_text(ui_TextAreaConfigPINNew, "");
    lv_textarea_set_text(ui_TextAreaConfigPINRepeat, "");
    lv_obj_clear_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_HIDDEN);
}

// Cancel button on PIN change panel clicked
void ui_event_button_config_pin_cancel_clicked(lv_event_t *)
{
    lv_obj_add_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(ui_TextAreaConfigPINCurrent, "");
    lv_textarea_set_text(ui_TextAreaConfigPINNew, "");
    lv_textarea_set_text(ui_TextAreaConfigPINRepeat, "");
}

// open keyboard for editing content of a textarea
void ui_event_textarea_number_clicked(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_keyboard_set_mode(ui_KeyboardConfig, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ui_KeyboardConfig, target);
    lv_obj_clear_flag(ui_KeyboardConfig, LV_OBJ_FLAG_HIDDEN);
}

// open keyboard for editing content of a textarea
void ui_event_textarea_text_clicked(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_keyboard_set_mode(ui_KeyboardConfig, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(ui_KeyboardConfig, target);
    lv_obj_clear_flag(ui_KeyboardConfig, LV_OBJ_FLAG_HIDDEN);
}

// Keyboard ready event
void ui_event_keyboard_config_ready(lv_event_t *e)
{
    lv_obj_add_flag(ui_KeyboardConfig, LV_OBJ_FLAG_HIDDEN);
}


void ui_event_ImageAboutClicked(lv_event_t *e)
{
    lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");
    ui_load_screen(ui_ScreenPin);

}


//  Button clicked on the selection screen
void ui_event_ButtonAboutClicked(lv_event_t *e)
{
    ui_selected_product = int(lv_event_get_user_data(e));

    if (bFreeDrink)
    {
        bFreeDrink = false;
        if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) ) {
            ui_queue_send_message(ADD_VOLUME, productConfig.getProduct(ui_selected_product)->getTapVolume());
            xSemaphoreGive(productConfigSemaphore);
        }
    }
    else
    {
        ui_queue_send_message(SHOW_MESSAGE_CREATING_INVOICE,15000);

        network_queue_send_message(TASK_NOTIFICATION_REQUEST_INVOICE,ui_selected_product);    
    }
}

// Cancel clicked on the PIN screen, go back to the Start page
void ui_event_button_pin_cancel_clicked(lv_event_t *e)
{
    lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");

    if (ui_PinEnteredPin.length() > 0)
    {
        ui_PinEnteredPin = "";
        ui_PinHidePin = "";
    }
    else
    {
        ui_load_screen(ui_ScreenAbout);
    }
}

// change pin event
void ui_event_button_config_pin_ok_clicked(lv_event_t *e)
{
    if (
        internal_verify_pin(lv_textarea_get_text(ui_TextAreaConfigPINCurrent)) &&
        (strlen(lv_textarea_get_text(ui_TextAreaConfigPINNew)) == 6) &&
        (strlen(lv_textarea_get_text(ui_TextAreaConfigPINRepeat)) == 6) &&
        (strcmp(lv_textarea_get_text(ui_TextAreaConfigPINNew), lv_textarea_get_text(ui_TextAreaConfigPINRepeat)) == 0))
    {
        ui_tapConfig.setPIN(lv_textarea_get_text(ui_TextAreaConfigPINNew));

        lv_obj_add_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(ui_TextAreaConfigPINCurrent, "");
        lv_textarea_set_text(ui_TextAreaConfigPINNew, "");
        lv_textarea_set_text(ui_TextAreaConfigPINRepeat, "");
    }
}

// verify the PIN and switch to Admin screen if correct
void ui_event_button_pin_ok_clicked(lv_event_t *e)
{
    // verify if the PIN is correct
    if (internal_verify_pin(ui_PinEnteredPin))
    {

        // reset the message
        lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");

        // start the update timer
        ui_load_screen(ui_ScreenAdmin);
    }
    else
    {
        // reset the message
        lv_label_set_text(ui_LabelPinMessage, "INCORRECT PIN");
    }

    // reset the stored values for the entered pin
    ui_PinEnteredPin = "";
    ui_PinHidePin = "";
}

// one of the number clicked on the PIN screen
void ui_event_button_pin_number_clicked(lv_event_t *e)
{
    int digit = int(lv_event_get_user_data(e));
    if (ui_PinEnteredPin.length() >= 6)
    {
        return;
    }

    ui_PinEnteredPin += digit;
    ui_PinHidePin += digit;

    lv_label_set_text(ui_LabelPinMessage, ui_PinHidePin.c_str());
}

// handle pressing of the tap button or anything on the tap screen
void ui_event_button_tap_start_clicked(lv_event_t *e)
{
    // remove instructions when a button is pressed
    lv_label_set_text(ui_LabelTapInstructions,"");

    if (strcmp(lv_label_get_text(ui_LabelTapStart), UI_BUTTON_TAP_TAP) == 0)
    {
        lv_obj_clear_flag(ui_BarTapProgress, LV_OBJ_FLAG_HIDDEN);
        internal_tap_resume();
        lv_label_set_text(ui_LabelTapStart, UI_BUTTON_TAP_PAUSE);
    }
    else if (strcmp(lv_label_get_text(ui_LabelTapStart), UI_BUTTON_TAP_PAUSE) == 0)
    {
        internal_tap_pause();
        lv_label_set_text(ui_LabelTapStart, UI_BUTTON_TAP_RESUME);
    }
    else if (strcmp(lv_label_get_text(ui_LabelTapStart), UI_BUTTON_TAP_RESUME) == 0)
    {
        internal_tap_resume();
        lv_label_set_text(ui_LabelTapStart, UI_BUTTON_TAP_PAUSE);
    }
}

void ui_event_button_payment_cancel_clicked(lv_event_t *e)
{
    ui_load_screen(ui_ScreenAbout);
}

void ui_event_button_payment_enter_pin_clicked(lv_event_t *e)
{
    ui_load_screen(ui_ScreenPin);
}

void ui_event_button_admin_open_clicked(lv_event_t *e)
{
    internal_tap_open();
}

void ui_event_button_admin_close_clicked(lv_event_t *e)
{
    internal_tap_close();
}

// get the current config and go to the config screen
void ui_event_button_admin_config_clicked(lv_event_t *e)
{
    char ui_text[10];

    // fill in data in the UI
    lv_textarea_set_text(ui_TextAreaConfigSSID, ui_tapConfig.getWiFiSSID());
    lv_textarea_set_text(ui_TextAreaConfigWifiPassword, ui_tapConfig.getWiFiPWD());
    lv_textarea_set_text(ui_TextAreaConfigLNbitsHost, ui_tapConfig.getLNbitsHost());
    lv_textarea_set_text(ui_TextAreaConfigDeviceID, ui_tapConfig.getDeviceID());
    snprintf_P(ui_text, sizeof(ui_text), PSTR("%d"), ui_tapConfig.getServoOpen());
    lv_textarea_set_text(ui_TextAreaConfigServoOpen, ui_text);
    snprintf_P(ui_text, sizeof(ui_text), PSTR("%d"), ui_tapConfig.getServoClose());
    lv_textarea_set_text(ui_TextAreaConfigServoClosed, ui_text);
    snprintf_P(ui_text, sizeof(ui_text), PSTR("%d"), ui_tapConfig.getVolumePerUnit());
    lv_textarea_set_text(ui_TextAreaConfigVolumePerUnit, ui_text);
    lv_dropdown_set_selected(ui_DropdownConfigControlMode, ui_tapConfig.getControlMode());
    lv_dropdown_set_selected(ui_DropdownConfigPaymentMode, ui_tapConfig.getPaymentMode());

    ui_load_screen(ui_ScreenConfig);

}

// when the free button is clicked on the admin screen, we'll allow a free drink
void ui_event_button_admin_free_clicked(lv_event_t *e)
{
    bFreeDrink = true;
    ui_load_screen(ui_ScreenAbout);
}

// when the calibrate button is clicked on the admin screen, we'll allow a free drink
// void ui_event_button_admin_calibrate_clicked(lv_event_t *e)
// {
//     bCalibrateTap = !bCalibrateTap;
// }



// cancel or Done button clicked on the admin screen navigates back to about
void ui_event_ButtonAdminCancelClicked(lv_event_t *e)
{
    ui_load_screen(ui_ScreenAbout);
}

// Initialize the screen that controls and displays the Tap process
void ui_ScreenTap_screen_init(void)
{
    // Initialize the Tap screen
    ui_ScreenTap = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenTap, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenTap, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenTap, 255, LV_STATE_DEFAULT);

    // ui_ImageTap = lv_img_create(ui_ScreenTap);
    // LV_IMG_DECLARE(tapinstructions);
    // lv_img_set_src(ui_ImageTap, &tapinstructions);
    // lv_obj_set_width(ui_ImageTap, LV_SIZE_CONTENT);
    // lv_obj_set_height(ui_ImageTap, LV_SIZE_CONTENT);
    // lv_obj_set_y(ui_ImageTap, 20);
    // lv_obj_set_align(ui_ImageTap, LV_ALIGN_TOP_MID);
    // lv_obj_add_flag(ui_ImageTap, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    // lv_obj_clear_flag(ui_ImageTap, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    ui_LabelTapInstructions = lv_label_create(ui_ScreenTap);
    lv_obj_set_width(ui_LabelTapInstructions, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelTapInstructions, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelTapInstructions, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_LabelTapInstructions, 20);
    lv_label_set_text(ui_LabelTapInstructions, "PUT YOUR GLASS UNDER THE TAP AND PRESS START");
    lv_obj_set_style_text_color(ui_LabelTapInstructions, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelTapInstructions, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelTapInstructions, &lv_font_montserrat_30, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_LabelTapInstructions, LV_TEXT_ALIGN_CENTER,0);

    ui_BarTapProgress = lv_bar_create(ui_ScreenTap);
    lv_bar_set_range(ui_BarTapProgress, 0, 10);
    lv_bar_set_value(ui_BarTapProgress, 0, LV_ANIM_OFF);

    static lv_style_t style_bar_bg;
    static lv_style_t style_bar_indicator;

    lv_style_init(&style_bar_bg);
    lv_style_set_border_color(&style_bar_bg, lv_color_hex(COLOR_BORDER));
    lv_style_set_border_width(&style_bar_bg, 2);
    lv_style_set_pad_all(&style_bar_bg, 6); /*To make the indicator smaller*/
    lv_style_set_radius(&style_bar_bg, 6);
    lv_style_set_height(&style_bar_bg, 40);
    lv_style_set_anim_time(&style_bar_bg, 1000);

    lv_style_init(&style_bar_indicator);
    lv_style_set_bg_opa(&style_bar_indicator, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bar_indicator, lv_color_hex(COLOR_BUTTON));
    lv_style_set_radius(&style_bar_indicator, 3);

    lv_obj_remove_style_all(ui_BarTapProgress);  /*To have a clean start*/
    lv_obj_add_style(ui_BarTapProgress, &style_bar_bg, 0);
    lv_obj_add_style(ui_BarTapProgress, &style_bar_indicator, LV_PART_INDICATOR);

    lv_obj_set_align(ui_BarTapProgress, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(ui_BarTapProgress, -20);

#ifdef PORTRAIT
    lv_obj_set_width(ui_BarTapProgress, 260);
#else
    lv_obj_set_width(ui_BarTapProgress, 440);
#endif
    
    
    
    // lv_obj_set_style_border_width(ui_BarTapProgress, 2, LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(ui_BarTapProgress, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_border_opa(ui_BarTapProgress, 255, LV_STATE_DEFAULT);
    // lv_obj_add_flag(ui_BarTapProgress, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_set_style_bg_color(ui_BarTapProgress, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    // lv_obj_set_style_border_color(ui_BarTapProgress, lv_color_hex(COLOR_BORDER), LV_STATE_DEFAULT);
    // // lv_obj_set_style_border_color(ui_BarTapProgress, lv_color_hex(COLOR_BORDER), LV_PART_INDICATOR);
    // lv_obj_set_style_radius(ui_BarTapProgress,0,LV_PART_INDICATOR);
    

    ui_ButtonTapStart = lv_btn_create(ui_ScreenTap);
#ifdef PORTRAIT
    lv_obj_set_width(ui_ButtonTapStart, 280);
    lv_obj_set_height(ui_ButtonTapStart, 100);
    lv_obj_set_x(ui_ButtonTapStart, 0);
    lv_obj_set_y(ui_ButtonTapStart, -50);
#else
    lv_obj_set_width(ui_ButtonTapStart,  280);
    lv_obj_set_height(ui_ButtonTapStart, 100);
#endif
    lv_obj_set_align(ui_ButtonTapStart, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonTapStart, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonTapStart, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonTapStart, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonTapStart, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ButtonTapStart,LV_OBJ_FLAG_HIDDEN);

    ui_LabelTapStart = lv_label_create(ui_ButtonTapStart);
    lv_obj_set_width(ui_LabelTapStart, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelTapStart, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelTapStart, LV_ALIGN_CENTER);
    // lv_obj_set_y(ui_LabelTapStart, -50);
    lv_label_set_text(ui_LabelTapStart, "HOLD YOUR GLASS UNDER THE TAP AND PRESS START");
    lv_obj_set_style_text_color(ui_LabelTapStart, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelTapStart, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelTapStart, &lv_font_montserrat_30, LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_ButtonTapStart, ui_event_button_tap_start_clicked, LV_EVENT_CLICKED, NULL);
}

// Initialize the Admin screen
void ui_ScreenAdmin_screen_init(void)
{
    int y = 0;
    ui_ScreenAdmin = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenAdmin, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenAdmin, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenAdmin, 255, LV_STATE_DEFAULT);

    y = 20;
    // ui_LabelAdminVersion = lv_label_create(ui_ScreenAdmin);
    // lv_obj_set_width(ui_LabelAdminVersion, LV_SIZE_CONTENT);  /// 1
    // lv_obj_set_height(ui_LabelAdminVersion, LV_SIZE_CONTENT); /// 1
    // lv_obj_set_x(ui_LabelAdminVersion, 10);
    // lv_obj_set_y(ui_LabelAdminVersion, y);
    // lv_label_set_text_fmt(ui_LabelAdminVersion, "Firmware version: %s", PSTR(FIRMWARE_VERSION));
    // lv_obj_set_style_text_color(ui_LabelAdminVersion, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui_LabelAdminVersion, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAdminVersion, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // y += 30;

    // ui_LabelAdminServerVersion = lv_label_create(ui_ScreenAdmin);
    // lv_obj_set_width(ui_LabelAdminServerVersion, LV_SIZE_CONTENT);  /// 1
    // lv_obj_set_height(ui_LabelAdminServerVersion, LV_SIZE_CONTENT); /// 1
    // lv_obj_set_x(ui_LabelAdminServerVersion, 10);
    // lv_obj_set_y(ui_LabelAdminServerVersion, y);
    // lv_label_set_text(ui_LabelAdminServerVersion, "Server version: ");
    // lv_obj_set_style_text_color(ui_LabelAdminServerVersion, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui_LabelAdminServerVersion, 255, LV_STATE_DEFAULT);
    // // lv_obj_set_style_text_font(ui_LabelAdminServerVersion, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // y += 30;

    // ui_LabelAdminBranding = lv_label_create(ui_ScreenAdmin);
    // lv_obj_set_width(ui_LabelAdminBranding, LV_SIZE_CONTENT);  /// 1
    // lv_obj_set_height(ui_LabelAdminBranding, LV_SIZE_CONTENT); /// 1
    // lv_obj_set_x(ui_LabelAdminBranding, 10);
    // lv_obj_set_y(ui_LabelAdminBranding, y);
    // lv_label_set_text(ui_LabelAdminBranding, "");
    // lv_obj_set_style_text_color(ui_LabelAdminBranding, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui_LabelAdminBranding, 255, LV_STATE_DEFAULT);
    // // lv_obj_set_style_text_font(ui_LabelAdminBranding, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // y += 30;

    ui_LabelAdminWiFiStatus = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminWiFiStatus, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminWiFiStatus, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelAdminWiFiStatus, 10);
    lv_obj_set_y(ui_LabelAdminWiFiStatus, y);
    lv_label_set_text(ui_LabelAdminWiFiStatus, "WiFi Status: ");
    lv_obj_set_style_text_color(ui_LabelAdminWiFiStatus, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminWiFiStatus, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAdminWiFiStatus, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    y += 30;

    ui_LabelAdminWebSocketStatus = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminWebSocketStatus, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminWebSocketStatus, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelAdminWebSocketStatus, 10);
    lv_obj_set_y(ui_LabelAdminWebSocketStatus, y);
    lv_label_set_text(ui_LabelAdminWebSocketStatus, "WebSocket Status: ");
    lv_obj_set_style_text_color(ui_LabelAdminWebSocketStatus, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminWebSocketStatus, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAdminWebSocketStatus, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    y += 30;

    // WiFi Strength
    ui_LabelAdminWiFiStrength = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminWiFiStrength, LV_SIZE_CONTENT);  /// 
    lv_obj_set_height(ui_LabelAdminWiFiStrength, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelAdminWiFiStrength, 10);
    lv_obj_set_y(ui_LabelAdminWiFiStrength, y);
    lv_label_set_text(ui_LabelAdminWiFiStrength, "WiFi Signal Strength: ");
    lv_obj_set_style_text_color(ui_LabelAdminWiFiStrength, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminWiFiStrength, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAdminWiFiStatus, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    y += 30;

    // Calibration data
    ui_LabelAdminCalibrationTicks = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminCalibrationTicks, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminCalibrationTicks, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelAdminCalibrationTicks, 10);
    lv_obj_set_y(ui_LabelAdminCalibrationTicks, y);
    lv_label_set_text(ui_LabelAdminCalibrationTicks, "Calibration ticks: ");
    lv_obj_set_style_text_color(ui_LabelAdminCalibrationTicks, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminCalibrationTicks, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAdminCalibrationTicks, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    y += 30;

    // free drink button
    ui_ButtonAdminFree = lv_btn_create(ui_ScreenAdmin);
#ifdef PORTRAIT
    lv_obj_set_width(ui_ButtonAdminFree, 300);
    lv_obj_set_height(ui_ButtonAdminFree, 70);
    lv_obj_set_x(ui_ButtonAdminFree, 10);
    lv_obj_set_y(ui_ButtonAdminFree, y);
    lv_obj_set_align(ui_ButtonAdminFree, LV_ALIGN_TOP_LEFT);
#else
    y = 170;
    lv_obj_set_width(ui_ButtonAdminFree, 140);
    lv_obj_set_height(ui_ButtonAdminFree, 70);
    lv_obj_set_x(ui_ButtonAdminFree, 10);
    lv_obj_set_y(ui_ButtonAdminFree, y);
    lv_obj_set_align(ui_ButtonAdminFree, LV_ALIGN_TOP_LEFT);
#endif
    lv_obj_add_flag(ui_ButtonAdminFree, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminFree, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminFree, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminFree, 255, LV_STATE_DEFAULT);

#ifdef CALIBRATION_BUTTON
    // // calibrate buttoin
    ui_ButtonAdminCalibrate = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminCalibrate, 140);
    lv_obj_set_height(ui_ButtonAdminCalibrate, 70);
    lv_obj_set_x(ui_ButtonAdminCalibrate, 160);
    lv_obj_set_y(ui_ButtonAdminCalibrate, y);
    lv_obj_set_align(ui_ButtonAdminCalibrate, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_ButtonAdminCalibrate, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminCalibrate, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminCalibrate, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminCalibrate, 255, LV_STATE_DEFAULT);
    // set the location for the next item
    y += 70 + 10;

    ui_LabelAdminCalibrate = lv_label_create(ui_ButtonAdminCalibrate);
    lv_obj_set_width(ui_LabelAdminCalibrate, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminCalibrate, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminCalibrate, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminCalibrate, "CALIB");
    lv_obj_set_style_text_color(ui_LabelAdminCalibrate, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminCalibrate, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminCalibrate, &ui_font_UbuntuBoldItalic, 0);
#endif




    y += 70 + 10;
  
    ui_LabelAdminFree = lv_label_create(ui_ButtonAdminFree);
    lv_obj_set_width(ui_LabelAdminFree, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminFree, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminFree, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminFree, "FREE");
    lv_obj_set_style_text_color(ui_LabelAdminFree, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminFree, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminFree, &ui_font_UbuntuBoldItalic, 0);

    // tap open and close buttons
    ui_ButtonAdminOpen = lv_btn_create(ui_ScreenAdmin);
#ifdef PORTRAIT
    lv_obj_set_width(ui_ButtonAdminOpen, 145);
    lv_obj_set_height(ui_ButtonAdminOpen, 70);
    lv_obj_set_x(ui_ButtonAdminOpen, -10);
    lv_obj_set_y(ui_ButtonAdminOpen, y);
    lv_obj_set_align(ui_ButtonAdminOpen, LV_ALIGN_TOP_RIGHT);
#else
    lv_obj_set_width(ui_ButtonAdminOpen, 140);
    lv_obj_set_height(ui_ButtonAdminOpen, 70);
    lv_obj_set_x(ui_ButtonAdminOpen, -10);
    lv_obj_set_y(ui_ButtonAdminOpen, 10);
    lv_obj_set_align(ui_ButtonAdminOpen, LV_ALIGN_TOP_RIGHT);
#endif
    lv_obj_add_flag(ui_ButtonAdminOpen, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminOpen, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminOpen, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminOpen, 255, LV_STATE_DEFAULT);
    

    ui_ButtonAdminClose = lv_btn_create(ui_ScreenAdmin);
#ifdef PORTRAIT
    lv_obj_set_width(ui_ButtonAdminClose, 145);
    lv_obj_set_height(ui_ButtonAdminClose, 70);
    lv_obj_set_x(ui_ButtonAdminClose, 10);
    lv_obj_set_y(ui_ButtonAdminClose, y);
    lv_obj_set_align(ui_ButtonAdminClose, LV_ALIGN_TOP_LEFT);
#else
    lv_obj_set_width(ui_ButtonAdminClose, 140);
    lv_obj_set_height(ui_ButtonAdminClose, 70);
    lv_obj_set_x(ui_ButtonAdminClose, -10);
    lv_obj_set_y(ui_ButtonAdminClose, 90);
    lv_obj_set_align(ui_ButtonAdminClose, LV_ALIGN_TOP_RIGHT);
#endif
    lv_obj_add_flag(ui_ButtonAdminClose, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminClose, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminClose, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminClose, 255, LV_STATE_DEFAULT);
    // set the location for the next item
    y += 70 + 10;

    ui_LabelAdminOpen = lv_label_create(ui_ButtonAdminOpen);
    lv_obj_set_width(ui_LabelAdminOpen, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminOpen, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminOpen, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminOpen, "OPEN");
    lv_obj_set_style_text_color(ui_LabelAdminOpen, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminOpen, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminOpen, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelAdminClose = lv_label_create(ui_ButtonAdminClose);
    lv_obj_set_width(ui_LabelAdminClose, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminClose, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminClose, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminClose, "CLOSE");
    lv_obj_set_style_text_color(ui_LabelAdminClose, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminClose, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminClose, &ui_font_UbuntuBoldItalic, 0);


    ui_ButtonAdminConfig = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminConfig, 95);
    lv_obj_set_height(ui_ButtonAdminConfig, 50);
    lv_obj_set_x(ui_ButtonAdminConfig, 10);
    lv_obj_set_y(ui_ButtonAdminConfig, -10);
    lv_obj_set_align(ui_ButtonAdminConfig, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonAdminConfig, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminConfig, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminConfig, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminConfig, 255, LV_STATE_DEFAULT);

    ui_LabelAdminConfig = lv_label_create(ui_ButtonAdminConfig);
    lv_obj_set_width(ui_LabelAdminConfig, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminConfig, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminConfig, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminConfig, "CONFIG");
    lv_obj_set_style_text_color(ui_LabelAdminConfig, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminConfig, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminConfig, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonAdminCancel = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminCancel, 95);
    lv_obj_set_height(ui_ButtonAdminCancel, 50);
    lv_obj_set_x(ui_ButtonAdminCancel, -10);
    lv_obj_set_y(ui_ButtonAdminCancel, -10);
    lv_obj_set_align(ui_ButtonAdminCancel, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonAdminCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAdminCancel, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminCancel, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAdminCancel, 255, LV_STATE_DEFAULT);

    ui_LabelAdminCancel = lv_label_create(ui_ButtonAdminCancel);
    lv_obj_set_width(ui_LabelAdminCancel, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAdminCancel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAdminCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminCancel, "DONE");
    lv_obj_set_style_text_color(ui_LabelAdminCancel, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAdminCancel, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAdminCancel, &ui_font_UbuntuBoldItalic, 0);

    lv_obj_add_event_cb(ui_ButtonAdminConfig, ui_event_button_admin_config_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminFree, ui_event_button_admin_free_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminCancel, ui_event_ButtonAdminCancelClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminOpen, ui_event_button_admin_open_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminClose, ui_event_button_admin_close_clicked, LV_EVENT_CLICKED, NULL);
    // lv_obj_add_event_cb(ui_ButtonAdminCalibrate, ui_event_button_admin_calibrate_clicked, LV_EVENT_CLICKED, NULL);
}

void ui_ScreenPin_screen_init(void)
{
    ui_ScreenPin = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPin, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenPin, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenPin, 255, LV_STATE_DEFAULT);
    
    ui_LabelPinMessage = lv_label_create(ui_ScreenPin);
    lv_obj_set_width(ui_LabelPinMessage, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPinMessage, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPinMessage, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");
    lv_obj_set_style_text_color(ui_LabelPinMessage, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPinMessage, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_LabelPinMessage, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_LabelPinMessage, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_LabelPinMessage, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelPinMessage, &lv_font_montserrat_36, 0);

#ifdef PORTRAIT
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_Y 0
#else
#define BUTTON_HEIGHT 60
#define BUTTON_WIDTH 100
#define BUTTON_Y -30
#endif

    ui_ButtonPin1 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin1, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin1, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin1, -10 - BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin1, BUTTON_Y - BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin1, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin1, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin1, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin1, 255, LV_STATE_DEFAULT);

    ui_LabelPin1 = lv_label_create(ui_ButtonPin1);
    lv_obj_set_width(ui_LabelPin1, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin1, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin1, "1");
    lv_obj_set_style_text_color(ui_LabelPin1, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin1, 255, LV_STATE_DEFAULT);

    ui_ButtonPin2 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin2, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin2, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin2, 0);
    lv_obj_set_y(ui_ButtonPin2, BUTTON_Y - BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin2, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin2, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin2, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin2, 255, LV_STATE_DEFAULT);

    ui_LabelPin2 = lv_label_create(ui_ButtonPin2);
    lv_obj_set_width(ui_LabelPin2, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin2, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin2, "2");
    lv_obj_set_style_text_color(ui_LabelPin2, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin2, 255, LV_STATE_DEFAULT);

    ui_ButtonPin3 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin3, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin3, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin3, 10 + BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin3, BUTTON_Y - BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin3, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin3, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin3, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin3, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin3, 255, LV_STATE_DEFAULT);

    ui_LabelPin3 = lv_label_create(ui_ButtonPin3);
    lv_obj_set_width(ui_LabelPin3, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin3, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin3, "3");
    lv_obj_set_style_text_color(ui_LabelPin3, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin3, 255, LV_STATE_DEFAULT);

    ui_ButtonPin4 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin4, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin4, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin4, -10 - BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin4, BUTTON_Y + 10);
    lv_obj_set_align(ui_ButtonPin4, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin4, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin4, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin4, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin4, 255, LV_STATE_DEFAULT);

    ui_LabelPin4 = lv_label_create(ui_ButtonPin4);
    lv_obj_set_width(ui_LabelPin4, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin4, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin4, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin4, "4");
    lv_obj_set_style_text_color(ui_LabelPin4, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin4, 255, LV_STATE_DEFAULT);

    ui_ButtonPin5 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin5, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin5, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin5, 0);
    lv_obj_set_y(ui_ButtonPin5, BUTTON_Y + 10);
    lv_obj_set_align(ui_ButtonPin5, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin5, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin5, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin5, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin5, 255, LV_STATE_DEFAULT);

    ui_LabelPin5 = lv_label_create(ui_ButtonPin5);
    lv_obj_set_width(ui_LabelPin5, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin5, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin5, "5");
    lv_obj_set_style_text_color(ui_LabelPin5, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin5, 255, LV_STATE_DEFAULT);

    ui_ButtonPin6 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin6, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin6, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin6, 10 + BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin6, BUTTON_Y + 10);
    lv_obj_set_align(ui_ButtonPin6, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin6, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin6, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin6, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin6, 255, LV_STATE_DEFAULT);

    ui_LabelPin6 = lv_label_create(ui_ButtonPin6);
    lv_obj_set_width(ui_LabelPin6, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin6, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin6, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin6, "6");
    lv_obj_set_style_text_color(ui_LabelPin6, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin6, 255, LV_STATE_DEFAULT);

    ui_ButtonPin7 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin7, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin7, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin7, -10 - BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin7, BUTTON_Y + 20 + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin7, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin7, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin7, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin7, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin7, 255, LV_STATE_DEFAULT);

    ui_LabelPin7 = lv_label_create(ui_ButtonPin7);
    lv_obj_set_width(ui_LabelPin7, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin7, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin7, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin7, "7");
    lv_obj_set_style_text_color(ui_LabelPin7, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin7, 255, LV_STATE_DEFAULT);

    ui_ButtonPin8 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin8, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin8, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin8, 0);
    lv_obj_set_y(ui_ButtonPin8, BUTTON_Y + 20 + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin8, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin8, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin8, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin8, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin8, 255, LV_STATE_DEFAULT);

    ui_LabelPin8 = lv_label_create(ui_ButtonPin8);
    lv_obj_set_width(ui_LabelPin8, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin8, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin8, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin8, "8");
    lv_obj_set_style_text_color(ui_LabelPin8, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin8, 255, LV_STATE_DEFAULT);

    ui_ButtonPin9 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin9, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin9, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin9, 10 + BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPin9, BUTTON_Y + 20 + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin9, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin9, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin9, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin9, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin9, 255, LV_STATE_DEFAULT);

    ui_LabelPin9 = lv_label_create(ui_ButtonPin9);
    lv_obj_set_width(ui_LabelPin9, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin9, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin9, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin9, "9");
    lv_obj_set_style_text_color(ui_LabelPin9, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin9, 255, LV_STATE_DEFAULT);

    ui_ButtonPinCancel = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPinCancel, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPinCancel, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPinCancel, -10 - BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPinCancel, BUTTON_Y + 30 + BUTTON_HEIGHT + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPinCancel, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPinCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPinCancel, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPinCancel, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPinCancel, 255, LV_STATE_DEFAULT);

    ui_LabelPINCancel = lv_label_create(ui_ButtonPinCancel);
    lv_obj_set_width(ui_LabelPINCancel, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPINCancel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPINCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPINCancel, "C");
    lv_obj_set_style_text_color(ui_LabelPINCancel, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPINCancel, 255, LV_STATE_DEFAULT);

    ui_ButtonPin0 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin0, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPin0, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPin0, 0);
    lv_obj_set_y(ui_ButtonPin0, BUTTON_Y + 30 + BUTTON_HEIGHT + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPin0, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin0, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPin0, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin0, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPin0, 255, LV_STATE_DEFAULT);

    ui_LabelPin0 = lv_label_create(ui_ButtonPin0);
    lv_obj_set_width(ui_LabelPin0, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPin0, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPin0, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin0, "0");
    lv_obj_set_style_text_color(ui_LabelPin0, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPin0, 255, LV_STATE_DEFAULT);

    ui_ButtonPinOK = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPinOK, BUTTON_WIDTH);
    lv_obj_set_height(ui_ButtonPinOK, BUTTON_HEIGHT);
    lv_obj_set_x(ui_ButtonPinOK, 10 + BUTTON_WIDTH);
    lv_obj_set_y(ui_ButtonPinOK, BUTTON_Y + 30 + BUTTON_HEIGHT + BUTTON_HEIGHT);
    lv_obj_set_align(ui_ButtonPinOK, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPinOK, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPinOK, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPinOK, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPinOK, 255, LV_STATE_DEFAULT);

    ui_LabelPinOK = lv_label_create(ui_ButtonPinOK);
    lv_obj_set_width(ui_LabelPinOK, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPinOK, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPinOK, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPinOK, "OK");
    lv_obj_set_style_text_color(ui_LabelPinOK, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPinOK, 255, LV_STATE_DEFAULT);
    
    // set font for all buttons
    lv_obj_set_style_text_font(ui_LabelPin0, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin1, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin2, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin3, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin4, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin5, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin6, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin7, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin8, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPin9, &lv_font_montserrat_36, 0);
     lv_obj_set_style_text_font(ui_LabelPinOK, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_font(ui_LabelPINCancel, &lv_font_montserrat_36, 0);


    lv_obj_add_event_cb(ui_ButtonPin0, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(ui_ButtonPin1, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(ui_ButtonPin2, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)2);
    lv_obj_add_event_cb(ui_ButtonPin3, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)3);
    lv_obj_add_event_cb(ui_ButtonPin4, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)4);
    lv_obj_add_event_cb(ui_ButtonPin5, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)5);
    lv_obj_add_event_cb(ui_ButtonPin6, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)6);
    lv_obj_add_event_cb(ui_ButtonPin7, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)7);
    lv_obj_add_event_cb(ui_ButtonPin8, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)8);
    lv_obj_add_event_cb(ui_ButtonPin9, ui_event_button_pin_number_clicked, LV_EVENT_CLICKED, (void *)9);
    lv_obj_add_event_cb(ui_ButtonPinCancel, ui_event_button_pin_cancel_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPinOK, ui_event_button_pin_ok_clicked, LV_EVENT_CLICKED, NULL);
}

void ui_ScreenAbout_screen_init(void)
{
    ui_ScreenAbout = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenAbout, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenAbout, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenAbout, 255, LV_STATE_DEFAULT);

    // ui_PanelAboutHeader = lv_obj_create(ui_ScreenAbout);
    // lv_obj_set_width(ui_PanelAboutHeader, gfx->width());
    // lv_obj_set_height(ui_PanelAboutHeader, 50);
    // lv_obj_clear_flag(ui_PanelAboutHeader, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    // lv_obj_set_style_radius(ui_PanelAboutHeader, 0, LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(ui_PanelAboutHeader, lv_color_hex(COLOR_HEADER), LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(ui_PanelAboutHeader, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_border_width(ui_PanelAboutHeader, 0, LV_STATE_DEFAULT);

    // ui_LabelAboutHeader = lv_label_create(ui_PanelAboutHeader);
    // lv_obj_set_width(ui_LabelAboutHeader, LV_SIZE_CONTENT);  /// 1
    // lv_obj_set_height(ui_LabelAboutHeader, LV_SIZE_CONTENT); /// 1
    // lv_obj_set_align(ui_LabelAboutHeader, LV_ALIGN_CENTER);
    // lv_label_set_text(ui_LabelAboutHeader, BB_NAME);
    // lv_obj_set_style_text_color(ui_LabelAboutHeader, lv_color_hex(COLOR_HEADER_TEXT), LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui_LabelAboutHeader, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAboutHeader, &ui_font_UbuntuBoldItalic, 0);

    ui_ImageAbout = lv_img_create(ui_ScreenAbout);
    lv_obj_add_flag(ui_ImageAbout,LV_OBJ_FLAG_CLICKABLE);
#ifdef BRANDING_BITCOIN_FRIESLAND
    LV_IMG_DECLARE(Bitcoin_Friesland);
    lv_img_set_src(ui_ImageAbout, &Bitcoin_Friesland);
#elif BRANDING_WHY2025
    LV_IMG_DECLARE(WHY2025_logo);
    lv_img_set_src(ui_ImageAbout, &WHY2025_logo);
#elif BRANDING_BITCOINTAPS
    LV_IMG_DECLARE(Image_BitcoinTaps_White);
    lv_img_set_src(ui_ImageAbout, &Image_BitcoinTaps_White);
    lv_obj_set_y(ui_ImageAbout,-20);
#else
    LV_IMG_DECLARE(kanhetal256);
    lv_img_set_src(ui_ImageAbout, &kanhetal256);
#endif

    lv_obj_set_width(ui_ImageAbout, LV_SIZE_CONTENT);  /// 256
    lv_obj_set_height(ui_ImageAbout, LV_SIZE_CONTENT); /// 256
    lv_obj_set_align(ui_ImageAbout, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ImageAbout, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(ui_ImageAbout, LV_OBJ_FLAG_SCROLLABLE); /// Flags


    ui_ButtonAboutOne = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutOne, 100);
    lv_obj_set_height(ui_ButtonAboutOne, 100);
    lv_obj_set_align(ui_ButtonAboutOne, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_ButtonAboutOne, 0);
    lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutOne, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAboutOne, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutOne = lv_label_create(ui_ButtonAboutOne);
    lv_obj_set_width(ui_LabelAboutOne, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAboutOne, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAboutOne, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutOne, "");
    lv_obj_set_style_text_color(ui_LabelAboutOne, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAboutOne, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelAboutOne, &ui_font_UbuntuBoldItalic, 0);
    lv_obj_set_style_text_font(ui_LabelAboutOne, &lv_font_montserrat_36, 0);


    ui_ButtonAboutTwo = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutTwo, 100);
    lv_obj_set_height(ui_ButtonAboutTwo, 100);
    lv_obj_set_align(ui_ButtonAboutTwo, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_ButtonAboutTwo, 0);
    lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutTwo, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAboutTwo, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutTwo = lv_label_create(ui_ButtonAboutTwo);
    lv_obj_set_width(ui_LabelAboutTwo, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAboutTwo, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAboutTwo, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutTwo, "");
    lv_obj_set_style_text_color(ui_LabelAboutTwo, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAboutTwo, 255, LV_STATE_DEFAULT);
    //lv_obj_set_style_text_font(ui_LabelAboutTwo, &ui_font_UbuntuBoldItalic, 0);
    lv_obj_set_style_text_font(ui_LabelAboutTwo, &lv_font_montserrat_30, 0);

    ui_ButtonAboutThree = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutThree, 100);
    lv_obj_set_height(ui_ButtonAboutThree, 100);
    lv_obj_set_align(ui_ButtonAboutThree, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_ButtonAboutThree, 0);
    lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutThree, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonAboutThree, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutThree = lv_label_create(ui_ButtonAboutThree);
    lv_obj_set_width(ui_LabelAboutThree, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAboutThree, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAboutThree, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutThree, "");
    lv_obj_set_style_text_color(ui_LabelAboutThree, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAboutThree, 255, LV_STATE_DEFAULT);
    //lv_obj_set_style_text_font(ui_LabelAboutThree, &ui_font_UbuntuBoldItalic, 0);
    lv_obj_set_style_text_font(ui_LabelAboutThree, &lv_font_montserrat_30, 0);



    ui_LabelAboutMessage = lv_label_create(ui_ScreenAbout);
    lv_obj_set_width(ui_LabelAboutMessage, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelAboutMessage, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelAboutMessage, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_LabelAboutMessage, "CONNECTING");
    lv_obj_set_y(ui_LabelAboutMessage,40);
    lv_obj_set_style_text_color(ui_LabelAboutMessage, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelAboutMessage, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelAboutMessage, &lv_font_montserrat_30, 0);

    // lv_obj_add_event_cb(ui_PanelAboutHeader, ui_event_PanelAboutHeaderClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ImageAbout, ui_event_ImageAboutClicked, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ButtonAboutOne, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(ui_ButtonAboutTwo, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(ui_ButtonAboutThree, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)2);
}

void ui_ScreenPayment_screen_init(void)
{
    ui_ScreenPayment = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPayment, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenPayment, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenPayment, 255, LV_STATE_DEFAULT);

    ui_LabelPaymentHeader = lv_label_create(ui_ScreenPayment);
    lv_obj_set_width(ui_LabelPaymentHeader, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPaymentHeader, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPaymentHeader, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_LabelPaymentHeader, 10);
    lv_label_set_text(ui_LabelPaymentHeader, "BitcoinTaps");
    lv_obj_set_style_text_color(ui_LabelPaymentHeader, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPaymentHeader, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelPaymentHeader, &lv_font_montserrat_36, 0);


    ui_ButtonPaymentCancel = lv_btn_create(ui_ScreenPayment);
#ifdef PORTRAIT
    lv_obj_set_width(ui_ButtonPaymentCancel, 120);
    lv_obj_set_height(ui_ButtonPaymentCancel, 60);
    lv_obj_set_x(ui_ButtonPaymentCancel, 0);
    lv_obj_set_y(ui_ButtonPaymentCancel, -10);
    lv_obj_set_align(ui_ButtonPaymentCancel, LV_ALIGN_BOTTOM_MID);
#else
    lv_obj_set_width(ui_ButtonPaymentCancel, 80);
    lv_obj_set_height(ui_ButtonPaymentCancel, 80);
    lv_obj_set_x(ui_ButtonPaymentCancel,10);
    lv_obj_set_y(ui_ButtonPaymentCancel,-10);
    lv_obj_set_align(ui_ButtonPaymentCancel, LV_ALIGN_BOTTOM_LEFT);
#endif
    lv_obj_add_flag(ui_ButtonPaymentCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPaymentCancel, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPaymentCancel, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPaymentCancel, 255, LV_STATE_DEFAULT);

    ui_LabelPaymentCancel = lv_label_create(ui_ButtonPaymentCancel);
    lv_obj_set_width(ui_LabelPaymentCancel, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPaymentCancel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPaymentCancel, LV_ALIGN_CENTER);
#ifdef PORTRAIT
    lv_label_set_text(ui_LabelPaymentCancel, "CANCEL");
#else
    lv_label_set_text(ui_LabelPaymentCancel, "<-");
#endif
    lv_obj_set_style_text_color(ui_LabelPaymentCancel, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPaymentCancel, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelPaymentCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPaymentEnterPin = lv_btn_create(ui_ScreenPayment);
    lv_obj_set_width(ui_ButtonPaymentEnterPin, 120);
    lv_obj_set_height(ui_ButtonPaymentEnterPin, 60);
    lv_obj_set_x(ui_ButtonPaymentEnterPin, -70);
    lv_obj_set_y(ui_ButtonPaymentEnterPin, -10);
    lv_obj_set_align(ui_ButtonPaymentEnterPin, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonPaymentEnterPin, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPaymentEnterPin, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPaymentEnterPin, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPaymentEnterPin, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ButtonPaymentEnterPin, LV_OBJ_FLAG_HIDDEN);

    ui_LabelPaymentEnterPin = lv_label_create(ui_ButtonPaymentEnterPin);
    lv_obj_set_width(ui_LabelPaymentEnterPin, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPaymentEnterPin, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPaymentEnterPin, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentEnterPin, "ENTER PIN");
    lv_obj_set_style_text_color(ui_LabelPaymentEnterPin, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPaymentEnterPin, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelPaymentEnterPin, &ui_font_UbuntuBoldItalic, 0);

    // initialize the QR code
    lv_color_t bg_color = lv_color_hex(0x000000);
    lv_color_t fg_color = lv_color_hex(0xFFFFFF);
    ui_QrcodePayment = lv_qrcode_create(ui_ScreenPayment);
#ifdef PORTRAIT
    lv_qrcode_set_size(ui_QrcodePayment, 300);
#else
    lv_qrcode_set_size(ui_QrcodePayment, 256);
#endif
    lv_qrcode_set_quiet_zone(ui_QrcodePayment,false);
    lv_qrcode_set_light_color(ui_QrcodePayment, fg_color);
    lv_qrcode_set_dark_color(ui_QrcodePayment, bg_color);
#ifdef PORTRAIT
    lv_obj_set_align(ui_QrcodePayment,LV_ALIGN_CENTER);
#else
    lv_obj_set_align(ui_QrcodePayment,LV_ALIGN_BOTTOM_MID);
#endif
    lv_obj_set_pos(ui_QrcodePayment, 0, 0);
    lv_obj_set_style_border_width(ui_QrcodePayment, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_QrcodePayment, lv_color_hex(COLOR_BORDER), LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_QrcodePayment, LV_OBJ_FLAG_HIDDEN);

    ui_PanelPaymentMessage = lv_obj_create(ui_ScreenPayment);
    lv_obj_set_y(ui_PanelPaymentMessage, 140);
    lv_obj_set_width(ui_PanelPaymentMessage, 256);
    lv_obj_set_height(ui_PanelPaymentMessage, 30);
    lv_obj_clear_flag(ui_PanelPaymentMessage, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_align(ui_PanelPaymentMessage, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_PanelPaymentMessage, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelPaymentMessage, lv_color_hex(COLOR_BORDER), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelPaymentMessage, 3, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelPaymentMessage, 255, LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_PanelPaymentMessage, LV_OBJ_FLAG_HIDDEN);

    ui_LabelPaymentMessage = lv_label_create(ui_PanelPaymentMessage);
    lv_obj_set_width(ui_LabelPaymentMessage, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelPaymentMessage, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelPaymentMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentMessage, "THIS IS A TEST");
    lv_obj_set_style_text_color(ui_LabelPaymentMessage, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelPaymentMessage, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelPaymentMessage, &ui_font_UbuntuBoldItalic, 0);

    lv_obj_add_event_cb(ui_ButtonPaymentEnterPin, ui_event_button_payment_enter_pin_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPaymentCancel, ui_event_button_payment_cancel_clicked, LV_EVENT_CLICKED, NULL);
}

void ui_ScreenConfig_screen_init(void)
{
    ui_ScreenConfig = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenConfig, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_ScreenConfig, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenConfig, 255, LV_STATE_DEFAULT);

    ui_LabelConfigSSID = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigSSID, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigSSID, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigSSID, 10);
    lv_obj_set_y(ui_LabelConfigSSID, 15);
    lv_label_set_text(ui_LabelConfigSSID, "SSID");
    lv_obj_set_style_text_color(ui_LabelConfigSSID, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigSSID, 255, LV_STATE_DEFAULT);
    //    lv_obj_set_style_text_font(ui_LabelConfigSSID, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigSSID = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigSSID, 240);
    lv_obj_set_height(ui_TextAreaConfigSSID, LV_SIZE_CONTENT); /// 50
    lv_obj_set_align(ui_TextAreaConfigSSID, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(ui_TextAreaConfigSSID, -10);
    lv_obj_set_y(ui_TextAreaConfigSSID, 5);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigSSID, "Wi-Fi SSID");
    lv_textarea_set_one_line(ui_TextAreaConfigSSID, true);

    ui_LabelConfigWifiPassword = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigWifiPassword, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigWifiPassword, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigWifiPassword, 10);
    lv_obj_set_y(ui_LabelConfigWifiPassword, 60);
    lv_label_set_text(ui_LabelConfigWifiPassword, "Secret");
    lv_obj_set_style_text_color(ui_LabelConfigWifiPassword, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigWifiPassword, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelConfigWifiPassword, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigWifiPassword = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigWifiPassword, 240);
    lv_obj_set_height(ui_TextAreaConfigWifiPassword, LV_SIZE_CONTENT); /// 70
    lv_obj_set_align(ui_TextAreaConfigWifiPassword, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(ui_TextAreaConfigWifiPassword, -10);
    lv_obj_set_y(ui_TextAreaConfigWifiPassword, 50);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigWifiPassword, "Wi-Fi password");
    lv_textarea_set_one_line(ui_TextAreaConfigWifiPassword, true);
    // lv_textarea_set_password_mode(ui_TextAreaConfigWifiPassword, true);

    ui_LabelConfigLNbitsHost = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigLNbitsHost, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigLNbitsHost, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigLNbitsHost, 10);
    lv_obj_set_y(ui_LabelConfigLNbitsHost, 105);
    lv_label_set_text(ui_LabelConfigLNbitsHost, "LNbits host");
    lv_obj_set_style_text_color(ui_LabelConfigLNbitsHost, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigLNbitsHost, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelConfigLNbitsHost, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigLNbitsHost = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigLNbitsHost, 200);
    lv_obj_set_height(ui_TextAreaConfigLNbitsHost, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigLNbitsHost, -10);
    lv_obj_set_y(ui_TextAreaConfigLNbitsHost, 95);
    lv_obj_set_align(ui_TextAreaConfigLNbitsHost, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigLNbitsHost, "LNbits host");
    lv_textarea_set_one_line(ui_TextAreaConfigLNbitsHost, true);

    ui_LabelConfigDeviceID = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigDeviceID, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigDeviceID, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigDeviceID, 10);
    lv_obj_set_y(ui_LabelConfigDeviceID, 150);
    lv_label_set_text(ui_LabelConfigDeviceID, "Device ID");
    lv_obj_set_style_text_color(ui_LabelConfigDeviceID, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigDeviceID, 255, LV_STATE_DEFAULT);
    //    lv_obj_set_style_text_font(ui_LabelConfigDeviceID, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigDeviceID = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigDeviceID, 200);
    lv_obj_set_height(ui_TextAreaConfigDeviceID, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigDeviceID, -10);
    lv_obj_set_y(ui_TextAreaConfigDeviceID, 140);
    lv_obj_set_align(ui_TextAreaConfigDeviceID, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigDeviceID, "Device ID");
    lv_textarea_set_one_line(ui_TextAreaConfigDeviceID, true);

    // Label for Volume per Unit Payload configuration
    ui_LabelConfigVolumePerUnit = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigVolumePerUnit, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigVolumePerUnit, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigVolumePerUnit, 10);
    lv_obj_set_y(ui_LabelConfigVolumePerUnit, 195);
    lv_label_set_text(ui_LabelConfigVolumePerUnit, "Ticks for 400ml");
    lv_obj_set_style_text_color(ui_LabelConfigVolumePerUnit, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigVolumePerUnit, 255, LV_STATE_DEFAULT);
    //    lv_obj_set_style_text_font(ui_LabelConfigVolumePerUnit, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigVolumePerUnit = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigVolumePerUnit, 95);
    lv_obj_set_height(ui_TextAreaConfigVolumePerUnit, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigVolumePerUnit, -10);
    lv_obj_set_y(ui_TextAreaConfigVolumePerUnit, 185);
    lv_obj_set_align(ui_TextAreaConfigVolumePerUnit, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigVolumePerUnit, "Enter a number");
    lv_textarea_set_one_line(ui_TextAreaConfigVolumePerUnit, true);

    ui_LabelConfigServo = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigServo, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigServo, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigServo, 10);
    lv_obj_set_y(ui_LabelConfigServo, 240);
    lv_label_set_text(ui_LabelConfigServo, "Servo");
    lv_obj_set_style_text_color(ui_LabelConfigServo, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigServo, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelConfigServo, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigServoClosed = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigServoClosed, 95);
    lv_obj_set_height(ui_TextAreaConfigServoClosed, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigServoClosed, 0);
    lv_obj_set_y(ui_TextAreaConfigServoClosed, 230);
    lv_obj_set_align(ui_TextAreaConfigServoClosed, LV_ALIGN_TOP_MID);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigServoClosed, "--");
    lv_textarea_set_one_line(ui_TextAreaConfigServoClosed, true);

    ui_TextAreaConfigServoOpen = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigServoOpen, 95);
    lv_obj_set_height(ui_TextAreaConfigServoOpen, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigServoOpen, -10);
    lv_obj_set_y(ui_TextAreaConfigServoOpen, 230);
    lv_obj_set_align(ui_TextAreaConfigServoOpen, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigServoOpen, "--");
    lv_textarea_set_one_line(ui_TextAreaConfigServoOpen, true);

    ui_ButtonPIN = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonPIN, 95);
    lv_obj_set_height(ui_ButtonPIN, 50);
    lv_obj_set_x(ui_ButtonPIN, 10);
    lv_obj_set_y(ui_ButtonPIN, 275);
    lv_obj_set_align(ui_ButtonPIN, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_ButtonPIN, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonPIN, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPIN, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonPIN, 255, LV_STATE_DEFAULT);

    ui_LabelConfigPIN = lv_label_create(ui_ButtonPIN);
    lv_obj_set_width(ui_LabelConfigPIN, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPIN, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigPIN, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigPIN, "PIN");
    lv_obj_set_style_text_color(ui_LabelConfigPIN, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigPIN, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigPIN, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigServoClose = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigServoClose, 95);
    lv_obj_set_height(ui_ButtonConfigServoClose, 50);
    lv_obj_set_x(ui_ButtonConfigServoClose, 0);
    lv_obj_set_y(ui_ButtonConfigServoClose, 275);
    lv_obj_set_align(ui_ButtonConfigServoClose, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_ButtonConfigServoClose, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigServoClose, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigServoClose, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigServoClose, 255, LV_STATE_DEFAULT);

    ui_LabelConfigServoClose = lv_label_create(ui_ButtonConfigServoClose);
    lv_obj_set_width(ui_LabelConfigServoClose, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigServoClose, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigServoClose, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigServoClose, "CLOSE");
    lv_obj_set_style_text_color(ui_LabelConfigServoClose, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigServoClose, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigServoClose, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigServoOpen = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigServoOpen, 95);
    lv_obj_set_height(ui_ButtonConfigServoOpen, 50);
    lv_obj_set_x(ui_ButtonConfigServoOpen, -10);
    lv_obj_set_y(ui_ButtonConfigServoOpen, 275);
    lv_obj_set_align(ui_ButtonConfigServoOpen, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigServoOpen, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigServoOpen, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigServoOpen, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigServoOpen, 255, LV_STATE_DEFAULT);

    ui_LabelConfigServoOpen = lv_label_create(ui_ButtonConfigServoOpen);
    lv_obj_set_width(ui_LabelConfigServoOpen, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigServoOpen, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigServoOpen, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigServoOpen, "OPEN");
    lv_obj_set_style_text_color(ui_LabelConfigServoOpen, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigServoOpen, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigServoOpen, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelConfigPaymentMode = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigPaymentMode, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPaymentMode, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigPaymentMode, 10);
    lv_obj_set_y(ui_LabelConfigPaymentMode, 345);
    lv_label_set_text(ui_LabelConfigPaymentMode, "Payment mode");
    lv_obj_set_style_text_color(ui_LabelConfigPaymentMode, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigPaymentMode, 255, LV_STATE_DEFAULT);
    // lv_obj_add_flag(ui_LabelConfigPaymentMode, LV_OBJ_FLAG_HIDDEN);
    // lv_obj_set_style_text_font(ui_LabelConfigPaymentMode, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Drop Down for Payment mode
    ui_DropdownConfigPaymentMode = lv_dropdown_create(ui_ScreenConfig);
    lv_dropdown_set_options(ui_DropdownConfigPaymentMode, "Wi-Fi\nOffline\nAuto\nSerial");
    lv_obj_set_width(ui_DropdownConfigPaymentMode, 170);
    lv_obj_set_height(ui_DropdownConfigPaymentMode, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_DropdownConfigPaymentMode, -10);
    lv_obj_set_y(ui_DropdownConfigPaymentMode, 335);
    lv_obj_set_align(ui_DropdownConfigPaymentMode, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_DropdownConfigPaymentMode, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    // lv_obj_add_flag(ui_DropdownConfigPaymentMode, LV_OBJ_FLAG_HIDDEN);

    // Label for control mode
    ui_LabelConfigControlMode = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigControlMode, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigControlMode, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigControlMode, 10);
    lv_obj_set_y(ui_LabelConfigControlMode, 390);
    lv_label_set_text(ui_LabelConfigControlMode, "Control mode");
    lv_obj_set_style_text_color(ui_LabelConfigControlMode, lv_color_hex(COLOR_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigControlMode, 255, LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui_LabelConfigControlMode, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    // Drop Down for switching mode
    ui_DropdownConfigControlMode = lv_dropdown_create(ui_ScreenConfig);
    lv_dropdown_set_options(ui_DropdownConfigControlMode, "None\nTime\nPulse");
    lv_obj_set_width(ui_DropdownConfigControlMode, 170);
    lv_obj_set_height(ui_DropdownConfigControlMode, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_DropdownConfigControlMode, -10);
    lv_obj_set_y(ui_DropdownConfigControlMode, 380);
    lv_obj_set_align(ui_DropdownConfigControlMode, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_DropdownConfigControlMode, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags

    ui_ButtonConfigSave = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigSave, 95);
    lv_obj_set_height(ui_ButtonConfigSave, 50);
    lv_obj_set_x(ui_ButtonConfigSave, -10);
    lv_obj_set_y(ui_ButtonConfigSave, -5);
    lv_obj_set_align(ui_ButtonConfigSave, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigSave, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigSave, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigSave, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigSave, 255, LV_STATE_DEFAULT);

    ui_LabelConfigDone = lv_label_create(ui_ButtonConfigSave);
    lv_obj_set_width(ui_LabelConfigDone, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigDone, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigDone, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigDone, "SAVE");
    lv_obj_set_style_text_color(ui_LabelConfigDone, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigDone, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigDone, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigCancel = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigCancel, 95);
    lv_obj_set_height(ui_ButtonConfigCancel, 50);
    lv_obj_set_x(ui_ButtonConfigCancel, 10);
    lv_obj_set_y(ui_ButtonConfigCancel, -5);
    lv_obj_set_align(ui_ButtonConfigCancel, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonConfigCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigCancel, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigCancel, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigCancel, 255, LV_STATE_DEFAULT);

    ui_LabelConfigCancel = lv_label_create(ui_ButtonConfigCancel);
    lv_obj_set_width(ui_LabelConfigCancel, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigCancel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigCancel, "CANCEL");
    lv_obj_set_style_text_color(ui_LabelConfigCancel, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigCancel, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigUpdate = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigUpdate, 95);
    lv_obj_set_height(ui_ButtonConfigUpdate, 50);
    lv_obj_set_x(ui_ButtonConfigUpdate, 0);
    lv_obj_set_y(ui_ButtonConfigUpdate, -5);
    lv_obj_set_align(ui_ButtonConfigUpdate, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonConfigUpdate, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigUpdate, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigUpdate, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigUpdate, 255, LV_STATE_DEFAULT);
    // lv_obj_add_flag(ui_ButtonConfigUpdate,LV_OBJ_FLAG_HIDDEN); // Hide the Update button for the moment

    ui_LabelConfigUpdate = lv_label_create(ui_ButtonConfigUpdate);
    lv_obj_set_width(ui_LabelConfigUpdate, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigUpdate, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigUpdate, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigUpdate, "UPDATE");
    lv_obj_set_style_text_color(ui_LabelConfigUpdate, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigUpdate, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigUpdate, &ui_font_UbuntuBoldItalic, 0);

    ui_PanelConfigPIN = lv_obj_create(ui_ScreenConfig);
    lv_obj_set_width(ui_PanelConfigPIN, 256);
    lv_obj_set_height(ui_PanelConfigPIN, 256);
    lv_obj_set_x(ui_PanelConfigPIN, 0);
    lv_obj_set_y(ui_PanelConfigPIN, 70);
    lv_obj_set_align(ui_PanelConfigPIN, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_HIDDEN);       /// Flags
    lv_obj_clear_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_PanelConfigPIN, lv_color_hex(COLOR_BACKGROUND), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelConfigPIN, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelConfigPIN, lv_color_hex(COLOR_BORDER), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelConfigPIN, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelConfigPIN, 2, LV_STATE_DEFAULT);

    ui_LabelConfigPINCurrent = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINCurrent, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPINCurrent, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigPINCurrent, 0);
    lv_obj_set_y(ui_LabelConfigPINCurrent, 10);
    lv_label_set_text(ui_LabelConfigPINCurrent, "Current");

    ui_LabelConfigPINNew = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINNew, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPINNew, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigPINNew, 0);
    lv_obj_set_y(ui_LabelConfigPINNew, 60);
    lv_label_set_text(ui_LabelConfigPINNew, "New");

    ui_LabelConfigPINRepeat = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINRepeat, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPINRepeat, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigPINRepeat, 0);
    lv_obj_set_y(ui_LabelConfigPINRepeat, 110);
    lv_label_set_text(ui_LabelConfigPINRepeat, "Repeat");

    ui_TextAreaConfigPINCurrent = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINCurrent, 150);
    lv_obj_set_height(ui_TextAreaConfigPINCurrent, LV_SIZE_CONTENT); /// 70
    lv_obj_set_align(ui_TextAreaConfigPINCurrent, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_accepted_chars(ui_TextAreaConfigPINCurrent, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINCurrent, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINCurrent, "Current PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINCurrent, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINCurrent, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINCurrent, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigPINRepeat = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINRepeat, 150);
    lv_obj_set_height(ui_TextAreaConfigPINRepeat, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigPINRepeat, 0);
    lv_obj_set_y(ui_TextAreaConfigPINRepeat, 100);
    lv_obj_set_align(ui_TextAreaConfigPINRepeat, LV_ALIGN_TOP_RIGHT);
    if ("0123456789" == "")
        lv_textarea_set_accepted_chars(ui_TextAreaConfigPINRepeat, NULL);
    else
        lv_textarea_set_accepted_chars(ui_TextAreaConfigPINRepeat, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINRepeat, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINRepeat, "Confirm PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINRepeat, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINRepeat, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINRepeat, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_TextAreaConfigPINNew = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINNew, 150);
    lv_obj_set_height(ui_TextAreaConfigPINNew, LV_SIZE_CONTENT); /// 70
    lv_obj_set_x(ui_TextAreaConfigPINNew, 0);
    lv_obj_set_y(ui_TextAreaConfigPINNew, 50);
    lv_obj_set_align(ui_TextAreaConfigPINNew, LV_ALIGN_TOP_RIGHT);
    if ("0123456789" == "")
        lv_textarea_set_accepted_chars(ui_TextAreaConfigPINNew, NULL);
    else
        lv_textarea_set_accepted_chars(ui_TextAreaConfigPINNew, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINNew, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINNew, "New PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINNew, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINNew, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINNew, &lv_font_montserrat_16, LV_STATE_DEFAULT);

    ui_ButtonConfigPINOK = lv_btn_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_ButtonConfigPINOK, 100);
    lv_obj_set_height(ui_ButtonConfigPINOK, 50);
    lv_obj_set_align(ui_ButtonConfigPINOK, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigPINOK, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigPINOK, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigPINOK, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigPINOK, 255, LV_STATE_DEFAULT);

    ui_LabelConfigButtonPINOK = lv_label_create(ui_ButtonConfigPINOK);
    lv_obj_set_width(ui_LabelConfigButtonPINOK, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigButtonPINOK, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigButtonPINOK, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigButtonPINOK, "OK");
    lv_obj_set_style_text_color(ui_LabelConfigButtonPINOK, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigButtonPINOK, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigButtonPINOK, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigPINCancel = lv_btn_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_ButtonConfigPINCancel, 100);
    lv_obj_set_height(ui_ButtonConfigPINCancel, 50);
    lv_obj_set_align(ui_ButtonConfigPINCancel, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonConfigPINCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(ui_ButtonConfigPINCancel, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigPINCancel, lv_color_hex(COLOR_BUTTON), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ButtonConfigPINCancel, 255, LV_STATE_DEFAULT);

    ui_LabelConfigButtonPINCancel = lv_label_create(ui_ButtonConfigPINCancel);
    lv_obj_set_width(ui_LabelConfigButtonPINCancel, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigButtonPINCancel, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(ui_LabelConfigButtonPINCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigButtonPINCancel, "CANCEL");
    lv_obj_set_style_text_color(ui_LabelConfigButtonPINCancel, lv_color_hex(COLOR_BUTTON_TEXT), LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelConfigButtonPINCancel, 255, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelConfigButtonPINCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelConfigPINMessage = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINMessage, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(ui_LabelConfigPINMessage, LV_SIZE_CONTENT); /// 1
    lv_obj_set_x(ui_LabelConfigPINMessage, 0);
    lv_obj_set_y(ui_LabelConfigPINMessage, -60);
    lv_obj_set_align(ui_LabelConfigPINMessage, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_LabelConfigPINMessage, "");

    ui_KeyboardConfig = lv_keyboard_create(ui_ScreenConfig);
    lv_obj_set_width(ui_KeyboardConfig, 320);
    lv_obj_set_height(ui_KeyboardConfig, 150);
    lv_obj_add_flag(ui_KeyboardConfig, LV_OBJ_FLAG_HIDDEN); /// Flags

    // // config events
    lv_obj_add_event_cb(ui_TextAreaConfigSSID, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigWifiPassword, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigLNbitsHost, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigDeviceID, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigServoClosed, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigServoOpen, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigVolumePerUnit, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_KeyboardConfig, ui_event_keyboard_config_ready, LV_EVENT_READY, NULL);

    lv_obj_add_event_cb(ui_ButtonPIN, ui_event_button_config_pin_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigServoClose, ui_event_button_config_servo_close_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigServoOpen, ui_event_button_config_servo_open_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ButtonConfigSave, ui_event_button_config_save_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigCancel, ui_event_button_config_cancel_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigUpdate, ui_event_button_config_update_clicked, LV_EVENT_CLICKED, NULL);

    // // PIN change panel events
    lv_obj_add_event_cb(ui_TextAreaConfigPINNew, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigPINCurrent, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigPINRepeat, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigPINOK, ui_event_button_config_pin_ok_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonConfigPINCancel, ui_event_button_config_pin_cancel_clicked, LV_EVENT_CLICKED, NULL);
}

// Callback so LVGL know the elapsed time
uint32_t millis_cb(void)
{
    return millis();
}

// LVGL calls it when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    // Call it to tell LVGL everthing is ready
    lv_disp_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    if (touch.touched())
    {
        // Read touched point from touch module
        touch.readData(&x, &y);

        // Set the coordinates
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void ui_flow_tap_timeout_cb(TimerHandle_t timer)
{
    ui_queue_send_message(FLOW_TAP_TIMEOUT,0);
}

void ui_hide_message_timer_cb(TimerHandle_t timer)
{
    ui_queue_send_message(HIDE_MESSAGE, 0);
}

void ui_back_to_about_timer_cb(TimerHandle_t timer)
{
    ui_queue_send_message(LOAD_ABOUT_SCREEN, 0);
}


void IRAM_ATTR pulseCounter()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(uiTaskHandle, 1, eIncrement, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ui_lvgl_timer_cb(TimerHandle_t timer)
{
    xTaskNotify(uiTaskHandle, TASK_NOTIFICATION_LVGL, eSetBits);
}

void ui_progress_timer_cb(TimerHandle_t timer)
{
    xTaskNotify(uiTaskHandle, TASK_NOTIFICATION_PROGRESS, eSetBits);
}


void ui_task(void *parameters)
{
    struct uiQueueMessage msg;
    
    // send the network the command to (re)start
    debug("ui_loop","calling network_restart");
    network_connect(ui_tapConfig);
    
    xTimerStart(lvglTimer, portMAX_DELAY);
    xTimerStart(progressTimer, portMAX_DELAY);
               

    while (1 == 1)
    {
        uint32_t ulNotifiedValue = 0;

        xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);


        // check for pulses or time pulses
        uint32_t flowValue = ulNotifiedValue & TASK_NOTIFICATION_PULSE_MASK;
        if (flowValue > 0) {
            if ( bAdminMode ) {                
                ui_tap_calibration_ticks += flowValue;
            } else {
                ui_tap_remaining_volume -= flowValue;
                
                xTimerReset(tapTimeoutTimer,portMAX_DELAY);
                if (ui_tap_remaining_volume <= 0) {
                    debug("ui_task","No volume remaining");
                    ui_tap_remaining_volume = 0;
                    internal_tap_close();

                    // no more volume to tap, hide the progress bar, say a goodbye message and start a timer yto go back to the start.
                    lv_obj_add_flag(ui_BarTapProgress, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_LabelTapInstructions, UI_LABEL_TAP_FINISHED);
                    lv_obj_add_flag(ui_ButtonTapStart, LV_OBJ_FLAG_HIDDEN);


                    // only start the timer when not active, otherwise we keep resetting the timer and never go back to the about screen.
                    if ( xTimerIsTimerActive(backToAboutTimer) == pdFALSE ) {              
                        xTimerReset(backToAboutTimer,portMAX_DELAY);
                    }
                }
            }
        }

        // check the queue
        if (xQueueReceive(uiQueue, &msg, 10))
        {
            switch (msg.type)
            {
            case ADD_VOLUME:
                // if not on the tap screeen init tap screen and navigate to the tap screen

                ui_tap_remaining_volume += (msg.data * ui_tapConfig.getVolumePerUnit() )/ 400;
                lv_bar_set_range(ui_BarTapProgress, 0, ui_tap_remaining_volume);
                xTimerReset(tapTimeoutTimer,portMAX_DELAY);

                
                if ( ui_tapConfig.getControlMode() == CONTROL_MODE_PULSE ) {
                    internal_tap_open();                
                }

                if ( lv_screen_active() != ui_ScreenTap ) {
                    ui_init_tap_screen();
                    ui_load_screen(ui_ScreenTap);
                }


                break;
            case SHOW_PAYMENT_BUTTONS:
                show_payment_buttons();
                break;
            case HIDE_PAYMENT_BUTTONS:
                hide_payment_buttons();
                break;
            case LOAD_ABOUT_SCREEN:
                ui_load_screen(ui_ScreenAbout);
                break;
            case HIDE_MESSAGE:
                lv_obj_add_flag(ui_PanelPaymentMessage, LV_OBJ_FLAG_HIDDEN);
                break;
            case INIT_AND_LOAD_PAYMENT_SCREEN:
                debug("ui_task","received init and load payment screen");
                init_and_load_payment_screen();
                break;
            case REPORT_WIFI_STATE:
                admin_report_wifi_state((wl_status_t)msg.data);
                break;
            case REPORT_WEBSOCKET_STATE:
                admin_report_websocket_state((NetworkWebSocketState)msg.data);
                break;
            case FLOW_TAP_TIMEOUT:
                ui_tap_remaining_volume = 0;
                internal_tap_close();

                // no more volume to tap, hide the progress bar, say a goodbye message and start a timer yto go back to the start.
                lv_obj_add_flag(ui_BarTapProgress, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_LabelTapInstructions, UI_LABEL_TAP_FINISHED);
                lv_obj_add_flag(ui_ButtonTapStart, LV_OBJ_FLAG_HIDDEN);


                // only start the timer when not active, otherwise we keep resetting the timer and never go back to the about screen.
                xTimerReset(backToAboutTimer,portMAX_DELAY);
                
                break;
            case OPEN_TAP:
                // open tap from admin panel
                internal_tap_open();
                break;
            case STOP_TAP:
                //close tap from admin panel
                internal_tap_close();
                break;
            case FREE_DRINK:
                // if we're in the payment screen, we give the current drink for free
                if (ui_ScreenPayment == lv_screen_active()) {   
                    if ( xSemaphoreTake(productConfigSemaphore,portMAX_DELAY) ) {
                        ui_queue_send_message(ADD_VOLUME, productConfig.getProduct(ui_selected_product)->getTapVolume());
                        xSemaphoreGive(productConfigSemaphore);
                    }   
                // otherwise, the next drink is free
                } else {
                    bFreeDrink = true;
                }
                break;
            case SHOW_MESSAGE_BAD_SERVER_RESPONSE:
            case SHOW_MESSAGE_CREATING_INVOICE:
            case SHOW_MESSAGE_FILE_NOT_FOUND:
            case SHOW_MESSAGE_HASH_ERROR:
            case SHOW_MESSAGE_INVALID_CARD:
            case SHOW_MESSAGE_INVOICE_PAID:
            case SHOW_MESSAGE_NFC_READ_ERROR:
            case SHOW_MESSAGE_NFC_READ_SUCCES:
            case SHOW_MESSAGE_NO_INVOICE_RECEIVED:
            case SHOW_MESSAGE_NO_PARTITION:
            case SHOW_MESSAGE_NOT_ALLOWED:
            case SHOW_MESSAGE_NOT_CONNECTED:
            case SHOW_MESSAGE_NOT_ENOUGH_SPACE:
            case SHOW_MESSAGE_PAYMENT_FAILED:
            case SHOW_MESSAGE_PROCESSING_MFC_PAYMENT:
            case SHOW_MESSAGE_READING_NFC:
            case SHOW_MESSAGE_RESTARTING:
            case SHOW_MESSAGE_SIZE_NOT_REPORTED:
            case SHOW_MESSAGE_UPDATE_PROGRESS:
            case SHOW_MESSAGE_VERIFY_FAILED:
            case SHOW_MESSAGE_WEBSOCKET_NOT_CONNECTED:
            case SHOW_MESSAGE_WIFI_NOT_CONNECTED:
            case SHOW_MESSAGE_WRONG_FLASH:
            case SHOW_MESSAGE_SSID_NOT_FOUND:
                set_message(msg);
                break;
            default:
                break;
            }
        }

        // update tap progress
        if (ulNotifiedValue & TASK_NOTIFICATION_PROGRESS)
        {
            update_tapprogress();
            ulNotifiedValue |= TASK_NOTIFICATION_LVGL;
        }

        // general lvgl handler
        if (ulNotifiedValue & TASK_NOTIFICATION_LVGL)
        {
            lv_task_handler();
            gfx->flush();
        }
    }
}

void ui_task_init()
{
    uiPaymentDataSemaphore = xSemaphoreCreateMutex();


    ui_tapConfig.load();

    // initialise servo and relay and close
    pinMode(RELAY_PIN, OUTPUT);
    ui_servo = new Servo();
    ui_servo->attach(SERVO_PIN);
    internal_tap_close();

    // initialias Pulse input for a flow meter
    pinMode(PULSE_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseCounter, FALLING);

    // Display setup
    if (!gfx->begin(40000000UL))
    {
        debug("ui_task_init","Failed to initialize display!");
        return;
    }

#ifdef PORTRAIT
    gfx->setRotation(0);
#else
    gfx->setRotation(1);
#endif
    // gfx->fillScreen(BLACK);

    // Switch backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Touch setup
    if (!touch.begin())
    {
        debug("ui_task_init","Failed to initialize touch module!");
        return;
    }

    touch.enOffsetCorrection(true);
#ifdef PORTRAIT
    touch.setRotation(0);
#else
    touch.setRotation(1);
#endif
    // touch.setOffsets(Touch_X_min, Touch_X_max, TFT_res_W-1, Touch_Y_min, Touch_Y_max, TFT_res_H-1);
    touch.setOffsets(12, 310, 319, 0, 479, 479);
    // Init LVGL
    lv_init();

    // Set a tick source so that LVGL will know how much time elapsed
    lv_tick_set_cb(millis_cb);

// Register print function for debugging
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    // Initialize the display buffer
    uint32_t screenWidth = gfx->width();
    uint32_t screenHeight = gfx->height();
    uint32_t bufSize = screenWidth * screenHeight / 10;
    lv_color_t *disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
        debug("ui_task_init","LVGL failed to allocate display buffer!");
        return;
    }

    // Initialize the display driver

    lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    // Initialize the input device driver
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    ui_ScreenAbout_screen_init();
    ui_ScreenPayment_screen_init();
    ui_ScreenConfig_screen_init();
    ui_ScreenPin_screen_init();
    ui_ScreenAdmin_screen_init();
    ui_ScreenTap_screen_init();

    lv_disp_load_scr(ui_ScreenAbout);


    // create queue for added volume
    uiQueue = xQueueCreate(5, sizeof(uiQueueMessage));
    if (uiQueue == NULL)
    {
        debug("ui_loop", "failed to created uiQueue");
    }

    // LVGL timer
    lvglTimer = xTimerCreate("LVGL timer", 25, pdTRUE, NULL, ui_lvgl_timer_cb);
    if (!lvglTimer)
    {
        debug("ui_loop","Failed to create lvglTimer");
    }

    // tap timeout timer
    tapTimeoutTimer = xTimerCreate("Tap Timeout Timer",pdMS_TO_TICKS(60000),pdFALSE,NULL,ui_flow_tap_timeout_cb);

    // progress bar timer
    progressTimer = xTimerCreate("Progress timer", pdMS_TO_TICKS(UI_TAP_PROGRESS_INTERVAL), pdTRUE, NULL, ui_progress_timer_cb);
    if (!progressTimer)
    {
        debug("ui_loop","Failed to create progressTimer");
    }
 
    // create a timer for hiding messages after a timeout controlled by this timer
    hideMessageTimer = xTimerCreate("hide message timer", pdMS_TO_TICKS(3000), pdFALSE, NULL, ui_hide_message_timer_cb);
    if (!hideMessageTimer)
    {
        debug("ui_loop", "Could not create hide message timer");
    }

    // create a timer for going back to the about page
    backToAboutTimer = xTimerCreate("back to about timer", pdMS_TO_TICKS(5000), pdFALSE, NULL, ui_back_to_about_timer_cb);
    if (!backToAboutTimer)
    {
        debug("ui_loop", "Could not create backTo about  timer");
    }


}

void ui_task_start() {

    // Start UI thread
    if ( xTaskCreatePinnedToCore (
        ui_task,     // Function to implement the task
        "ui_loop",   // Name of the task
        10000,       //  Stack size in bytes
        NULL,      // Task input parameter
        5,         // Priority of the task
        &uiTaskHandle,      // Task handle.
        1          // Core where the task should run
    ) != pdPASS ) {
        debug("setup","Failed to start UI thread");
    }


}
