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

static SemaphoreHandle_t lvgl_sem;
TapConfig ui_tapConfig;
String ui_PinEnteredPin = ""; // The actual entered PIN on the PIN Screen
String ui_PinHidePin = ""; // a string of stars displayed on the screen
unsigned long ui_message_millis = 0; // millis when a message has been set
bool bFreeDrink = false;
Servo *ui_servo;
bool ui_tap_running = false; 
int ui_tap_remaining_credit = 0;
int ui_tap_timeout = 0;
unsigned long previousRunTime;

#define UI_BUTTON_TAP_TAP "PRESS TO START"
#define UI_BUTTON_TAP_PAUSE "PRESS TO PAUSE"
#define UI_BUTTON_TAP_RESUME "PRESS TO RESUME"
#define UI_LABEL_TAP_FINISHED "ENJOY YOUR DRINK!"
#define UI_LABEL_TAP_INSTRUCTION "TILT GLASS SLIGHTLY"
#define UI_TAP_PROGRESS_INTERVAL 200

#define runEvery(t) for (static uint16_t _lasttime; millis() - _lasttime >= (t); _lasttime += (t)) 

//
Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, TFT_res_W, TFT_res_H);
Arduino_Canvas *gfx = new Arduino_Canvas(TFT_res_W, TFT_res_H, g, 0, 0, TFT_rot);
AXS15231B_Touch touch(Touch_SCL, Touch_SDA, Touch_INT, Touch_ADDR, TFT_rot);

// Declare the main font
LV_FONT_DECLARE(ui_font_UbuntuBoldItalic);

// the tap screen
lv_obj_t * ui_ScreenTap;
lv_obj_t * ui_ImageTap;
lv_obj_t * ui_LabelTapInstructions;
lv_obj_t * ui_LabelTapStart;
lv_obj_t * ui_BarTapProgress;
lv_obj_t * ui_ButtonTapStart;

// the admin screen
lv_obj_t * ui_ScreenAdmin;
lv_obj_t * ui_PanelAdminHeader;
lv_obj_t * ui_LabelAdminHeader;
lv_obj_t * ui_LabelAdminVersion;
lv_obj_t * ui_LabelAdminServerVersion;
lv_obj_t * ui_LabelAdminBranding;
lv_obj_t * ui_LabelAdminWiFiStatus;
lv_obj_t * ui_LabelAdminWebSocketStatus;
lv_obj_t * ui_ButtonAdminConfig;
lv_obj_t * ui_LabelAdminConfig;
lv_obj_t * ui_ButtonAdminCancel;
lv_obj_t * ui_LabelAdminCancel;
lv_obj_t * ui_ButtonAdminFree;
lv_obj_t * ui_LabelAdminFree;
lv_obj_t * ui_ButtonAdminOpen;
lv_obj_t * ui_LabelAdminOpen;
lv_obj_t * ui_ButtonAdminClose;
lv_obj_t * ui_LabelAdminClose;


// the enter pin screen
lv_obj_t * ui_ScreenPin;
lv_obj_t * ui_PanelPinHeader;
lv_obj_t * ui_LabelPinHeader;
lv_obj_t * ui_LabelPinMessage;
lv_obj_t * ui_ButtonPin0;
lv_obj_t * ui_LabelPin0;
lv_obj_t * ui_ButtonPin1;
lv_obj_t * ui_LabelPin1;
lv_obj_t * ui_ButtonPin2;
lv_obj_t * ui_LabelPin2;
lv_obj_t * ui_ButtonPin3;
lv_obj_t * ui_LabelPin3;
lv_obj_t * ui_ButtonPin4;
lv_obj_t * ui_LabelPin4;
lv_obj_t * ui_ButtonPin5;
lv_obj_t * ui_LabelPin5;
lv_obj_t * ui_ButtonPin6;
lv_obj_t * ui_LabelPin6;
lv_obj_t * ui_ButtonPin7;
lv_obj_t * ui_LabelPin7;
lv_obj_t * ui_ButtonPin8;
lv_obj_t * ui_LabelPin8;
lv_obj_t * ui_ButtonPin9;
lv_obj_t * ui_LabelPin9;
lv_obj_t * ui_ButtonPinCancel;
lv_obj_t * ui_LabelPINCancel;
lv_obj_t * ui_ButtonPinOK;
lv_obj_t * ui_LabelPinOK;

// the about screen
lv_obj_t *ui_ScreenAbout;
lv_obj_t * ui_PanelAboutHeader;
lv_obj_t * ui_LabelAboutHeader;
lv_obj_t * ui_ImageAbout;
lv_obj_t * ui_LabelAboutStatus;
lv_obj_t * ui_PanelAboutMessage;
lv_obj_t * ui_LabelAboutMessage;
lv_obj_t * ui_ButtonAboutOne;
lv_obj_t * ui_ButtonAboutTwo;
lv_obj_t * ui_ButtonAboutThree;
lv_obj_t * ui_LabelAboutOne;
lv_obj_t * ui_LabelAboutTwo;
lv_obj_t * ui_LabelAboutThree;
LV_IMG_DECLARE(ui_img_bitcointaps256);

// the payment screen
lv_obj_t *ui_ScreenPayment;
lv_obj_t *ui_PanelPaymentHeader;
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


#define BB_FGCOLOR 0xFFFFFF
#define BB_BGCOLOR 0x4E4D4D
#ifdef BRANDING_WHY2025
#define BB_NAME "Why Drink? Why Not!"
#else
#define BB_NAME "BitcoinTaps"
#endif
#define FIRMWARE_VERSION "test"

void internal_servo_write(int i) {
#ifdef DEBUG
    Serial.printf("[internal_servo_write] i = %d\n",i);    
#endif
    ui_servo->write(i);
}

void internal_tap_open() {
    internal_servo_write(ui_tapConfig.getServoOpen());
}

void internal_tap_close() {
    internal_servo_write(ui_tapConfig.getServoClose());
}

// close the tap
// called from UI thread, so lock before accessing variables
void internal_tap_pause() {
    Serial.println("[internal_tap_pause]");
    ui_tap_running = false;
    internal_tap_close();
}

// open the tap
// called from UI thread, so lock before accessing variables
void internal_tap_resume() {
    Serial.println("[internal_tap_resume]");
    ui_tap_running = true;
    internal_tap_open();
}

// Tap progress
// running in the main loop process, do not lock here
void internal_update_tapprogress() {
    // Serial.printf("[updateTapProgress]  %d %d\n",ui_tap_remaining_credit,ui_tap_timeout);


    if ( ui_tap_remaining_credit <= 0 ) {
        ui_tap_remaining_credit = 0;
        if ( ui_tap_timeout <= 0 ) {
            if (ui_ScreenTap == lv_screen_active()) {
                lv_disp_load_scr(ui_ScreenAbout);        
            }
            return;
        }
        ui_tap_timeout--;
        return;
    }




    //Serial.printf("[updateTapProgress] tap credit = %d, timeout = %d\n",ui_tap_remaining_credit,ui_tap_timeout);


    // we have credit, load the tap screen if not loaded
    if ( ui_ScreenTap != lv_screen_active() ) {
        ui_tap_running = false;
        lv_label_set_text(ui_LabelTapInstructions,UI_LABEL_TAP_INSTRUCTION);
        lv_label_set_text(ui_LabelTapStart,UI_BUTTON_TAP_TAP);
        lv_obj_clear_flag(ui_BarTapProgress,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ButtonTapStart,LV_OBJ_FLAG_HIDDEN);
        lv_disp_load_scr(ui_ScreenTap);        
    } else if (!(lv_obj_is_visible(ui_BarTapProgress)&&lv_obj_is_visible(ui_ButtonTapStart))) {
        ui_tap_running = false;
        lv_label_set_text(ui_LabelTapInstructions,UI_LABEL_TAP_INSTRUCTION);
        lv_label_set_text(ui_LabelTapStart,UI_BUTTON_TAP_TAP);
        lv_obj_clear_flag(ui_BarTapProgress,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_ButtonTapStart,LV_OBJ_FLAG_HIDDEN);
    }

    lv_bar_set_value(ui_BarTapProgress,ui_tap_remaining_credit,LV_ANIM_OFF);
    
    // if the tap is running
    if ( ui_tap_running  ) {
        ui_tap_remaining_credit-= UI_TAP_PROGRESS_INTERVAL;
        lv_bar_set_value(ui_BarTapProgress,ui_tap_remaining_credit,LV_ANIM_OFF);
    
        if ( ui_tap_remaining_credit <= 0 ) {
#ifdef DEBUG
            Serial.printf("[updateTapProgress] No credit remaining\n");
#endif
            internal_tap_close();
            
            lv_obj_add_flag(ui_BarTapProgress,LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonTapStart,LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui_LabelTapInstructions,UI_LABEL_TAP_FINISHED);
            
            ui_tap_timeout = 3000 / UI_TAP_PROGRESS_INTERVAL;            
        }
    }

}

// called from UI thread, so lock before accessing variables
bool internal_verify_pin(String entered_pin) {
#ifdef DEBUG
    Serial.println("[internal_verify_pin]");
#endif    
    String pin = ui_tapConfig.getPIN();
     
    if (( pin.length() == 6 )&&(entered_pin.length() == 6)&&(pin.equals(entered_pin))) {
        return true;
    } else {
        return false;
    }

}



bool ui_add_tap_credit(int credit) {
    if ( xSemaphoreTake(lvgl_sem,200) == pdTRUE ) {
        ui_tap_remaining_credit += credit;        
        lv_bar_set_range(ui_BarTapProgress,0,ui_tap_remaining_credit);
        xSemaphoreGive(lvgl_sem);            
        return true;
    }
    return false;
}


// update the state of the websocket in the admin screen
bool ui_update_websocket_state(const char *s) {
    if ( xSemaphoreTake(lvgl_sem,400) == pdTRUE ) {
        lv_label_set_text_fmt(ui_LabelAdminWebSocketStatus,"WebSocket Status: %s",s);
        xSemaphoreGive(lvgl_sem);            
        return true;
    } 
    return false;
}

// update the status string of the wifi connection in the admin screeen
bool ui_update_wifi_state(const char *s) {
    if ( xSemaphoreTake(lvgl_sem,400) == pdTRUE ) {
        lv_label_set_text_fmt(ui_LabelAdminWiFiStatus,"WiFi Status: %s",s);
        xSemaphoreGive(lvgl_sem);            
        return true;
    }    
    return false;
}

// update the server version string in the admin screen
bool ui_update_server_version(const char *s) {
    if ( xSemaphoreTake(lvgl_sem,300) == pdTRUE ) {
        lv_label_set_text_fmt(ui_LabelAdminServerVersion,"Server version: %s",s);
        xSemaphoreGive(lvgl_sem);   
        return true;         
    }
    return false;
}

// update the server branding in the admin screen
bool ui_update_server_branding(const char *s) {
    if ( xSemaphoreTake(lvgl_sem,200) == pdTRUE ) {
        lv_label_set_text_fmt(ui_LabelAdminBranding,"Server branding: %s",s);
        xSemaphoreGive(lvgl_sem);            
        return true;
    }
    return false;
}

void internal_set_payment_message(const char *s,int delay) {
    lv_label_set_text(ui_LabelAboutMessage, s);
    lv_label_set_text(ui_LabelPaymentMessage,s);
    lv_obj_clear_flag(ui_PanelPaymentMessage,LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);
    ui_message_millis = millis() + delay * 1000;
}

// set the message of the payment status on the payment screen
bool ui_set_payment_message(const char *s,int delay) {
    if ( xSemaphoreTake(lvgl_sem,400) == pdTRUE ) {
        internal_set_payment_message(s,delay);
        xSemaphoreGive(lvgl_sem);            
        return false;
    }            
    return true;
}


void internal_hide_payment_message() {
    ui_message_millis = 0;
    lv_obj_add_flag(ui_PanelPaymentMessage,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);
}

bool ui_hide_payment_message() {
    if ( xSemaphoreTake(lvgl_sem,400) == pdTRUE ) {
        internal_hide_payment_message();
        xSemaphoreGive(lvgl_sem);   
        return true;         
    }
    return false;
}

// display the qr code on the payment screen
bool ui_display_qrcode(const char *s,int len) {
    if ( xSemaphoreTake(lvgl_sem,500) == pdTRUE ) {
        lv_obj_clear_flag(ui_QrcodePayment, LV_OBJ_FLAG_HIDDEN);
        lv_qrcode_update(ui_QrcodePayment, s, len);
        xSemaphoreGive(lvgl_sem);        
        return true;
    }
    return false;
}

// navigate to the payment screen
bool ui_load_payment_screen(int amount, const char *currency) {
    if ( xSemaphoreTake(lvgl_sem,500) == pdTRUE ) {
        lv_disp_load_scr(ui_ScreenPayment);        
        lv_label_set_text_fmt(ui_LabelPaymentHeader,"PAY %d %s",amount,currency);
        xSemaphoreGive(lvgl_sem);        
        return true;
    }
    return false;
}


bool ui_hide_payment_buttons() {
    if ( xSemaphoreTake(lvgl_sem,300) == pdTRUE ) {
        lv_obj_add_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
        xSemaphoreGive(lvgl_sem);        
        return true;
    }
    return false;
}

// initialise buttons on the about page
bool ui_init_switches(int num,const char *label[]) {
#ifdef DEBUG    
    Serial.println("[ui_init_switches]");
#endif

    if ( xSemaphoreTake(lvgl_sem,700) == pdTRUE ) {
        switch ( num ) {
            case 1:
                lv_obj_clear_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_LabelAboutOne,label[0]);
                lv_obj_set_width(ui_ButtonAboutOne, 256);
                lv_obj_set_x(ui_ButtonAboutOne, 0);
                break;
            case 2:
                lv_obj_clear_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_LabelAboutOne,label[0]);
                lv_label_set_text(ui_LabelAboutTwo,label[1]);
                lv_obj_set_x(ui_ButtonAboutOne, -70);
                lv_obj_set_x(ui_ButtonAboutTwo, 70);
                lv_obj_set_width(ui_ButtonAboutOne, 120);
                lv_obj_set_width(ui_ButtonAboutTwo, 120);
                break;
            case 3:
                lv_obj_clear_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(ui_LabelAboutOne,label[0]);
                lv_label_set_text(ui_LabelAboutTwo,label[1]);
                lv_label_set_text(ui_LabelAboutThree,label[2]);
                lv_obj_set_x(ui_ButtonAboutOne, -105);
                lv_obj_set_x(ui_ButtonAboutTwo, 0);
                lv_obj_set_x(ui_ButtonAboutThree, 105);
                lv_obj_set_width(ui_ButtonAboutOne, 100);
                lv_obj_set_width(ui_ButtonAboutTwo, 100);
                lv_obj_set_width(ui_ButtonAboutThree, 100);
                break;
            default:
                lv_obj_add_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
                break;
        }        
        xSemaphoreGive(lvgl_sem);        
        return true;
    }
    return false;
}

// return true if the current screen is the payment screen
bool ui_payment_screen_active() {
    bool b = false;
    if ( xSemaphoreTake(lvgl_sem,400) == pdTRUE ) {
        if ( ui_ScreenPayment == lv_screen_active() ) {
            b = true;
        }
        xSemaphoreGive(lvgl_sem);        
    }    
    return b;
}

// return true if the current screen is the admin screen
bool ui_admin_screen_active() {
    bool b = false;
    if ( xSemaphoreTake(lvgl_sem,500) == pdTRUE ) {
        if ( ui_ScreenAdmin == lv_screen_active() ) {
            b = true;
        }
        xSemaphoreGive(lvgl_sem);        
    }    
    return b;
}

// open the about screen
bool ui_load_about_screen() {
    if ( xSemaphoreTake(lvgl_sem,500) == pdTRUE ) {
        lv_disp_load_scr(ui_ScreenAbout);        
        xSemaphoreGive(lvgl_sem);   
        return true;     
    }
    return false;
}

// from here down are the ui events

// Close button on the config screen clicked
void ui_event_button_config_servo_close_clicked(lv_event_t *e) {
    int value = atoi(lv_textarea_get_text(ui_TextAreaConfigServoClosed));
    internal_servo_write(value);
}

// Open button on the config screen clicked
void ui_event_button_config_servo_open_clicked(lv_event_t *e) {
    int value = atoi(lv_textarea_get_text(ui_TextAreaConfigServoOpen));
    internal_servo_write(value);
}

// update button clicked
void ui_event_button_config_update_clicked(lv_event_t * e) {
    lv_disp_load_scr(ui_ScreenAbout);        
    lv_obj_add_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_PanelAboutHeader,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ui_LabelAboutMessage,"UPDATING FIRMWARE");
    lv_obj_clear_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(ui_PanelAboutMessage,150);

    main_firmware_update();
}

// Button event to cancel config without saving changes
void ui_event_button_config_cancel_clicked(lv_event_t * e) {
    // make call to main to start updating
    lv_disp_load_scr(ui_ScreenAdmin);
}

// Button event to cancel config without saving changes
void ui_event_button_config_save_clicked(lv_event_t * e) {
    char buf[20];

    ui_tapConfig.setWiFiSSID(lv_textarea_get_text(ui_TextAreaConfigSSID));
    ui_tapConfig.setWiFiPWD(lv_textarea_get_text(ui_TextAreaConfigWifiPassword));
    ui_tapConfig.setLNbitsHost(lv_textarea_get_text(ui_TextAreaConfigLNbitsHost));
    ui_tapConfig.setDeviceID(lv_textarea_get_text(ui_TextAreaConfigDeviceID));
    ui_tapConfig.setServoOpen(atoi(lv_textarea_get_text(ui_TextAreaConfigServoOpen)));
    ui_tapConfig.setServoClose(atoi(lv_textarea_get_text(ui_TextAreaConfigServoClosed)));
    
    lv_dropdown_get_selected_str(ui_DropdownConfigPaymentMode,buf,sizeof(buf));
	if ( strncasecmp(buf,"online",6) == 0 ) {
    	ui_tapConfig.setPaymentMode(PAYMENT_MODE_ONLINE);
  	} else if ( strncasecmp(buf,"offline",7) == 0 ) {
    	ui_tapConfig.setPaymentMode(PAYMENT_MODE_OFFLINE);
  	} else if ( strncasecmp(buf,"auto",4) == 0 ) {
    	ui_tapConfig.setPaymentMode(PAYMENT_MODE_AUTO);
  	}

	lv_dropdown_get_selected_str(ui_DropdownConfigControlMode, buf, sizeof(buf));
	if ( strncasecmp(buf,"Time",4) == 0 ) {
    	ui_tapConfig.setControlMode(CONTROL_MODE_TIME);
  	} else if ( strncasecmp(buf,"Credit",6) == 0 ) {
    	ui_tapConfig.setControlMode(CONTROL_MODE_CREDIT);
  	} else {
    	ui_tapConfig.setControlMode(CONTROL_MODE_NONE);
 	}

    ui_tapConfig.save();

    // copy updated config to main
    // update and restart network 
    network_restart(ui_tapConfig.getWiFiSSID(),ui_tapConfig.getWiFiPWD(),ui_tapConfig.getLNbitsHost(),ui_tapConfig.getDeviceID());
    
    // go back to the admin screen
    lv_disp_load_scr(ui_ScreenAdmin);
}

// pPIN button on the config clicked
void ui_event_button_config_pin_clicked(lv_event_t *) {
    lv_textarea_set_text(ui_TextAreaConfigPINCurrent,"");
    lv_textarea_set_text(ui_TextAreaConfigPINNew,"");
    lv_textarea_set_text(ui_TextAreaConfigPINRepeat,"");        
    lv_obj_clear_flag(ui_PanelConfigPIN,LV_OBJ_FLAG_HIDDEN);
}

// Cancel button on PIN change panel clicked
void ui_event_button_config_pin_cancel_clicked(lv_event_t *) {
    lv_obj_add_flag(ui_PanelConfigPIN,LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(ui_TextAreaConfigPINCurrent,"");
    lv_textarea_set_text(ui_TextAreaConfigPINNew,"");
    lv_textarea_set_text(ui_TextAreaConfigPINRepeat,"");        
}

// open keyboard for editing content of a textarea
void ui_event_textarea_number_clicked(lv_event_t * e) 
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_keyboard_set_mode(ui_KeyboardConfig, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ui_KeyboardConfig,target);
    lv_obj_clear_flag(ui_KeyboardConfig,LV_OBJ_FLAG_HIDDEN);
}

// open keyboard for editing content of a textarea
void ui_event_textarea_text_clicked(lv_event_t * e) 
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_keyboard_set_mode(ui_KeyboardConfig, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(ui_KeyboardConfig,target);
    lv_obj_clear_flag(ui_KeyboardConfig,LV_OBJ_FLAG_HIDDEN);
}

// Keyboard ready event
void ui_event_keyboard_config_ready(lv_event_t * e) 
{
    lv_obj_add_flag(ui_KeyboardConfig,LV_OBJ_FLAG_HIDDEN);
}

void ui_event_PanelAboutHeaderClicked(lv_event_t * e) {
    lv_label_set_text(ui_LabelPinHeader, "ENTER PIN");
    lv_disp_load_scr(ui_ScreenPin);
}

//  Button clicked on the selection screen
void ui_event_ButtonAboutClicked(lv_event_t * e) {
#ifdef DEBUG
    Serial.println("[ui_event_ButtonAboutClicked]");
#endif
    int product = int(lv_event_get_user_data(e));

    if ( bFreeDrink ) {
        bFreeDrink = false;
        lv_disp_load_scr(ui_ScreenTap);
    } else {
        internal_set_payment_message("CREATING INVOICE",15);
        main_get_invoice(product);
    }
}

// Cancel clicked on the PIN screen, go back to the Start page
void ui_event_ButtonPinCancelClicked(lv_event_t * e) {
    lv_label_set_text(ui_LabelPinHeader, "ENTER PIN");
    lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");

    if ( ui_PinEnteredPin.length() > 0 ) {
        ui_PinEnteredPin = "";
        ui_PinHidePin = "";
    } else {
        lv_disp_load_scr(ui_ScreenAbout);
    }
}

// change pin event
void ui_event_button_config_pin_ok_clicked(lv_event_t * e) {
    if ( 
        internal_verify_pin(lv_textarea_get_text(ui_TextAreaConfigPINCurrent)) &&
        (strlen(lv_textarea_get_text(ui_TextAreaConfigPINNew)) == 6) &&
        (strlen(lv_textarea_get_text(ui_TextAreaConfigPINRepeat)) == 6) &&
        (strcmp(lv_textarea_get_text(ui_TextAreaConfigPINNew),lv_textarea_get_text(ui_TextAreaConfigPINRepeat))==0)) {
        ui_tapConfig.setPIN(lv_textarea_get_text(ui_TextAreaConfigPINNew));

        lv_obj_add_flag(ui_PanelConfigPIN,LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(ui_TextAreaConfigPINCurrent,"");
        lv_textarea_set_text(ui_TextAreaConfigPINNew,"");
        lv_textarea_set_text(ui_TextAreaConfigPINRepeat,"");        
    }
}

// verify the PIN and switch to Admin screen if correct
void ui_event_ButtonPinOKClicked(lv_event_t * e) {
    // verify if the PIN is correct
    if ( internal_verify_pin(ui_PinEnteredPin) ) {
    
        // reset the message
        lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");

        // Load the config screen
        lv_disp_load_scr(ui_ScreenAdmin);
    } else {
        // reset the message
        lv_label_set_text(ui_LabelPinMessage, "INCORRECT PIN");
    }

    // reset the stored values for the entered pin
    ui_PinEnteredPin = "";
    ui_PinHidePin = "";
}

// one of the number clicked on the PIN screen
void ui_event_ButtonPinNumberClicked(lv_event_t * e) {
    int digit = int(lv_event_get_user_data(e));
    if ( ui_PinEnteredPin.length() >= 6 ) {
        return;
    }

    ui_PinEnteredPin += digit;
    ui_PinHidePin += digit;

    lv_label_set_text(ui_LabelPinMessage,ui_PinHidePin.c_str());
}

// handle pressing of the tap button or anything on the tap screen
void ui_event_ButtonTapStartClicked(lv_event_t * e) {
    // TODO: ADd debounce to prevent servo damage
    if ( strcmp(lv_label_get_text(ui_LabelTapStart),UI_BUTTON_TAP_TAP) == 0 ) {
        lv_obj_clear_flag(ui_BarTapProgress,LV_OBJ_FLAG_HIDDEN);
        internal_tap_resume();
        lv_label_set_text(ui_LabelTapStart,UI_BUTTON_TAP_PAUSE);
    } else if ( strcmp(lv_label_get_text(ui_LabelTapStart),UI_BUTTON_TAP_PAUSE) == 0) {
        internal_tap_pause();
        lv_label_set_text(ui_LabelTapStart,UI_BUTTON_TAP_RESUME);
    } else if ( strcmp(lv_label_get_text(ui_LabelTapStart),UI_BUTTON_TAP_RESUME) == 0) {
        internal_tap_resume();
        lv_label_set_text(ui_LabelTapStart,UI_BUTTON_TAP_PAUSE);
    }
}


void ui_event_ButtonPaymentCancelClicked(lv_event_t * e) {
    lv_obj_add_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);
    lv_disp_load_scr(ui_ScreenAbout);
}

void ui_event_ButtonPaymentEnterPinClicked(lv_event_t * e) {
    lv_disp_load_scr(ui_ScreenPin);
}

void ui_event_button_admin_open_clicked(lv_event_t * e) {
#ifdef DEBUG
    Serial.println("[ui_event_button_admin_open_clicked]");
#endif
    internal_tap_open();
}

void ui_event_button_admin_close_clicked(lv_event_t * e) {
#ifdef DEBUG
    Serial.println("[ui_event_button_admin_close_clicked]");
#endif
    internal_tap_close();
}

void ui_event_button_admin_free_clicked(lv_event_t * e) {
    bFreeDrink = true;
    lv_obj_add_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);
    lv_disp_load_scr(ui_ScreenAbout);
}

// get the current config and go to the config screen
void ui_event_ButtonAdminConfigClicked(lv_event_t * e) {
    char servo_text[10];
    
    // fill in data in the UI
    lv_textarea_set_text(ui_TextAreaConfigSSID,ui_tapConfig.getWiFiSSID());
    lv_textarea_set_text(ui_TextAreaConfigWifiPassword,ui_tapConfig.getWiFiPWD());
    lv_textarea_set_text(ui_TextAreaConfigLNbitsHost,ui_tapConfig.getLNbitsHost());
    lv_textarea_set_text(ui_TextAreaConfigDeviceID,ui_tapConfig.getDeviceID());
    snprintf_P(servo_text, sizeof(servo_text), PSTR("%d"), ui_tapConfig.getServoOpen());
    lv_textarea_set_text(ui_TextAreaConfigServoOpen,servo_text);
    snprintf_P(servo_text, sizeof(servo_text), PSTR("%d"), ui_tapConfig.getServoClose());
    lv_textarea_set_text(ui_TextAreaConfigServoClosed,servo_text);
    lv_dropdown_set_selected(ui_DropdownConfigControlMode,ui_tapConfig.getControlMode());
    lv_dropdown_set_selected(ui_DropdownConfigPaymentMode,ui_tapConfig.getPaymentMode());


    lv_disp_load_scr(ui_ScreenConfig);

}

// cancel or Done button clicked on the admin screen navigates back to about
void ui_event_ButtonAdminCancelClicked(lv_event_t * e) {    
    lv_disp_load_scr(ui_ScreenAbout);
}


// Initialize the screen that controls and displays the Tap process
void ui_ScreenTap_screen_init(void) {
    // Initialize the Tap screen
    ui_ScreenTap = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenTap, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenTap, lv_color_hex(BB_FGCOLOR),LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenTap, 255,LV_STATE_ANY);


    ui_ImageTap = lv_img_create(ui_ScreenTap);
    LV_IMG_DECLARE(tapinstructions);
    lv_img_set_src(ui_ImageTap, &tapinstructions); 
    lv_obj_set_width(ui_ImageTap, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ImageTap, LV_SIZE_CONTENT); 
    lv_obj_set_y(ui_ImageTap, 20);
    lv_obj_set_align(ui_ImageTap, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_ImageTap, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_ImageTap, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_LabelTapInstructions = lv_label_create(ui_ScreenTap);
    lv_obj_set_width(ui_LabelTapInstructions, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTapInstructions, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelTapInstructions, LV_ALIGN_TOP_MID);
    lv_obj_set_y(ui_LabelTapInstructions, 320);
    lv_label_set_text(ui_LabelTapInstructions, UI_LABEL_TAP_INSTRUCTION);
    lv_obj_set_style_text_color(ui_LabelTapInstructions, lv_color_hex(BB_BGCOLOR),LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelTapInstructions, 255,LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelTapInstructions, &ui_font_UbuntuBoldItalic,LV_STATE_ANY);


    ui_BarTapProgress = lv_bar_create(ui_ScreenTap);
    lv_bar_set_range(ui_BarTapProgress,0,10);
    lv_bar_set_value(ui_BarTapProgress, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_BarTapProgress, 260);
    lv_obj_set_height(ui_BarTapProgress, 10);    
    //lv_obj_set_x(ui_BarTapProgress, 0);
    lv_obj_set_y(ui_BarTapProgress, -20);
    lv_obj_set_align(ui_BarTapProgress, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(ui_BarTapProgress, lv_color_hex(BB_FGCOLOR),LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_BarTapProgress, 255,LV_STATE_ANY);
    lv_obj_set_style_border_color(ui_BarTapProgress, lv_color_hex(BB_FGCOLOR),LV_STATE_ANY);
    lv_obj_set_style_border_opa(ui_BarTapProgress, 255,LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_BarTapProgress, 2,LV_STATE_ANY);
    //lv_obj_set_style_radius(ui_BarTapProgress,0,LV_PART_ANY);
    
    lv_obj_add_flag(ui_BarTapProgress,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ui_BarTapProgress, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_BarTapProgress, 255, LV_STATE_ANY);

    ui_ButtonTapStart = lv_btn_create(ui_ScreenTap);
    lv_obj_set_width(ui_ButtonTapStart, 260);
    lv_obj_set_height(ui_ButtonTapStart,50);
    lv_obj_set_x(ui_ButtonTapStart, 0);
    lv_obj_set_y(ui_ButtonTapStart, -50);
    lv_obj_set_align(ui_ButtonTapStart, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonTapStart, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonTapStart, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonTapStart, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonTapStart, 255, LV_STATE_ANY);


    ui_LabelTapStart = lv_label_create(ui_ButtonTapStart);
    lv_obj_set_width(ui_LabelTapStart, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTapStart, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelTapStart, LV_ALIGN_CENTER);
    // lv_obj_set_y(ui_LabelTapStart, -50);
    lv_label_set_text(ui_LabelTapStart, UI_BUTTON_TAP_TAP);
    lv_obj_set_style_text_color(ui_LabelTapStart, lv_color_hex(BB_BGCOLOR),LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelTapStart, 255,LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelTapStart, &ui_font_UbuntuBoldItalic,LV_STATE_ANY);

    lv_obj_add_event_cb(ui_ButtonTapStart, ui_event_ButtonTapStartClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ScreenTap, ui_event_ButtonTapStartClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_BarTapProgress, ui_event_ButtonTapStartClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_LabelTapInstructions, ui_event_ButtonTapStartClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ImageTap, ui_event_ButtonTapStartClicked, LV_EVENT_CLICKED, NULL);

}




// Initialize the Admin screen
void ui_ScreenAdmin_screen_init(void)
{
    ui_ScreenAdmin = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenAdmin, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenAdmin, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenAdmin, 255, LV_STATE_ANY);

    ui_PanelAdminHeader = lv_obj_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_PanelAdminHeader, 320);
    lv_obj_set_height(ui_PanelAdminHeader, 50);
    lv_obj_clear_flag(ui_PanelAdminHeader, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_PanelAdminHeader, 0, LV_STATE_ANY);
    lv_obj_set_style_bg_color(ui_PanelAdminHeader, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelAdminHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelAdminHeader, 0, LV_STATE_ANY);

    ui_LabelAdminHeader = lv_label_create(ui_PanelAdminHeader);
    lv_obj_set_width(ui_LabelAdminHeader, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminHeader, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminHeader, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminHeader, "Administration");
    lv_obj_set_style_text_color(ui_LabelAdminHeader, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminHeader, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelAdminVersion = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminVersion, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminVersion, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAdminVersion, 10);
    lv_obj_set_y(ui_LabelAdminVersion, 70);
    lv_label_set_text_fmt(ui_LabelAdminVersion, "Firmware version: %s",PSTR(FIRMWARE_VERSION));
    lv_obj_set_style_text_color(ui_LabelAdminVersion, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminVersion, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAdminVersion, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_LabelAdminServerVersion = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminServerVersion, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminServerVersion, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAdminServerVersion, 10);
    lv_obj_set_y(ui_LabelAdminServerVersion, 100);
    lv_label_set_text(ui_LabelAdminServerVersion, "Server version: ");
    lv_obj_set_style_text_color(ui_LabelAdminServerVersion, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminServerVersion, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAdminServerVersion, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_LabelAdminBranding = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminBranding, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminBranding, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAdminBranding, 10);
    lv_obj_set_y(ui_LabelAdminBranding, 130);
    lv_label_set_text(ui_LabelAdminBranding, "");
    lv_obj_set_style_text_color(ui_LabelAdminBranding, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminBranding, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAdminBranding, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_LabelAdminWiFiStatus = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminWiFiStatus, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminWiFiStatus, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAdminWiFiStatus, 10);
    lv_obj_set_y(ui_LabelAdminWiFiStatus, 160);
    lv_label_set_text(ui_LabelAdminWiFiStatus, "WiFi Status: ");
    lv_obj_set_style_text_color(ui_LabelAdminWiFiStatus, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminWiFiStatus, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAdminWiFiStatus, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_LabelAdminWebSocketStatus = lv_label_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_LabelAdminWebSocketStatus, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminWebSocketStatus, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAdminWebSocketStatus, 10);
    lv_obj_set_y(ui_LabelAdminWebSocketStatus, 190);
    lv_label_set_text(ui_LabelAdminWebSocketStatus, "WebSocket Status: ");
    lv_obj_set_style_text_color(ui_LabelAdminWebSocketStatus, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminWebSocketStatus, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAdminWebSocketStatus, &lv_font_montserrat_16, LV_STATE_ANY);


    ui_ButtonAdminOpen = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminOpen, 145);
    lv_obj_set_height(ui_ButtonAdminOpen, 70);
    lv_obj_set_x(ui_ButtonAdminOpen, -10);
    lv_obj_set_y(ui_ButtonAdminOpen, 220);
    lv_obj_set_align(ui_ButtonAdminOpen, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_ButtonAdminOpen, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAdminOpen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminOpen, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAdminOpen, 255, LV_STATE_ANY);

    ui_LabelAdminOpen = lv_label_create(ui_ButtonAdminOpen);
    lv_obj_set_width(ui_LabelAdminOpen, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminOpen, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminOpen, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminOpen, "OPEN");
    lv_obj_set_style_text_color(ui_LabelAdminOpen, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminOpen, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminOpen, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonAdminClose = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminClose, 145);
    lv_obj_set_height(ui_ButtonAdminClose, 70);
    lv_obj_set_x(ui_ButtonAdminClose, 10);
    lv_obj_set_y(ui_ButtonAdminClose, 220);
    lv_obj_set_align(ui_ButtonAdminClose, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_ButtonAdminClose, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAdminClose, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminClose, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAdminClose, 255, LV_STATE_ANY);

    ui_LabelAdminClose = lv_label_create(ui_ButtonAdminClose);
    lv_obj_set_width(ui_LabelAdminClose, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminClose, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminClose, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminClose, "CLOSE");
    lv_obj_set_style_text_color(ui_LabelAdminClose, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminClose, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminClose, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonAdminFree = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminFree, 300);
    lv_obj_set_height(ui_ButtonAdminFree, 70);
    lv_obj_set_x(ui_ButtonAdminFree, 10);
    lv_obj_set_y(ui_ButtonAdminFree, 300);
    lv_obj_set_align(ui_ButtonAdminFree, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_ButtonAdminFree, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAdminFree, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminFree, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAdminFree, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_ButtonAdminFree,LV_OBJ_FLAG_HIDDEN);

    ui_LabelAdminFree = lv_label_create(ui_ButtonAdminFree);
    lv_obj_set_width(ui_LabelAdminFree, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminFree, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminFree, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminFree, "FREE");
    lv_obj_set_style_text_color(ui_LabelAdminFree, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminFree, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminFree, &ui_font_UbuntuBoldItalic, 0);


    ui_ButtonAdminConfig = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminConfig, 100);
    lv_obj_set_height(ui_ButtonAdminConfig, 50);
    lv_obj_set_x(ui_ButtonAdminConfig, 10);
    lv_obj_set_y(ui_ButtonAdminConfig, -10);
    lv_obj_set_align(ui_ButtonAdminConfig, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonAdminConfig, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAdminConfig, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminConfig, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAdminConfig, 255, LV_STATE_ANY);

    ui_LabelAdminConfig = lv_label_create(ui_ButtonAdminConfig);
    lv_obj_set_width(ui_LabelAdminConfig, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminConfig, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminConfig, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminConfig, "CONFIG");
    lv_obj_set_style_text_color(ui_LabelAdminConfig, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminConfig, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminConfig, &ui_font_UbuntuBoldItalic, 0);


    ui_ButtonAdminCancel = lv_btn_create(ui_ScreenAdmin);
    lv_obj_set_width(ui_ButtonAdminCancel, 100);
    lv_obj_set_height(ui_ButtonAdminCancel, 50);
    lv_obj_set_x(ui_ButtonAdminCancel, -10);
    lv_obj_set_y(ui_ButtonAdminCancel, -10);
    lv_obj_set_align(ui_ButtonAdminCancel, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonAdminCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAdminCancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAdminCancel, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAdminCancel, 255, LV_STATE_ANY);

    ui_LabelAdminCancel = lv_label_create(ui_ButtonAdminCancel);
    lv_obj_set_width(ui_LabelAdminCancel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdminCancel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdminCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdminCancel, "DONE");
    lv_obj_set_style_text_color(ui_LabelAdminCancel, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAdminCancel, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAdminCancel, &ui_font_UbuntuBoldItalic, 0);


    lv_obj_add_event_cb(ui_ButtonAdminConfig, ui_event_ButtonAdminConfigClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminCancel, ui_event_ButtonAdminCancelClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminOpen, ui_event_button_admin_open_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminClose, ui_event_button_admin_close_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonAdminFree, ui_event_button_admin_free_clicked, LV_EVENT_CLICKED, NULL);

}


void ui_ScreenPin_screen_init(void)
{
    ui_ScreenPin = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPin, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenPin, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenPin, 255, LV_STATE_ANY);

    ui_PanelPinHeader = lv_obj_create(ui_ScreenPin);
    lv_obj_set_width(ui_PanelPinHeader, 320);
    lv_obj_set_height(ui_PanelPinHeader, 50);
    lv_obj_clear_flag(ui_PanelPinHeader, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_PanelPinHeader, 0, LV_STATE_ANY);
    lv_obj_set_style_bg_color(ui_PanelPinHeader, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelPinHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelPinHeader, 0, LV_STATE_ANY);

    ui_LabelPinMessage = lv_label_create(ui_ScreenPin);
    lv_obj_set_width(ui_LabelPinMessage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPinMessage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelPinMessage, 0);
    lv_obj_set_y(ui_LabelPinMessage, 80);
    lv_obj_set_align(ui_LabelPinMessage, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_LabelPinMessage, "ENTER PIN");
    lv_obj_set_style_text_color(ui_LabelPinMessage, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPinMessage, 255, LV_STATE_ANY);
    lv_obj_set_style_text_letter_space(ui_LabelPinMessage, 10, LV_STATE_ANY);
    lv_obj_set_style_text_line_space(ui_LabelPinMessage, 0, LV_STATE_ANY);
    lv_obj_set_style_text_align(ui_LabelPinMessage, LV_TEXT_ALIGN_LEFT, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPinMessage, &ui_font_UbuntuBoldItalic, 0);


    ui_LabelPinHeader = lv_label_create(ui_PanelPinHeader);
    lv_obj_set_width(ui_LabelPinHeader, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPinHeader, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPinHeader, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPinHeader, "ENTER PIN");
    lv_obj_set_style_text_color(ui_LabelPinHeader, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPinHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPinHeader, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin1 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin1, 80);
    lv_obj_set_height(ui_ButtonPin1, 80);
    lv_obj_set_x(ui_ButtonPin1, -90);
    lv_obj_set_y(ui_ButtonPin1, -80);
    lv_obj_set_align(ui_ButtonPin1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin1, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin1, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin1, 255, LV_STATE_ANY);

    ui_LabelPin1 = lv_label_create(ui_ButtonPin1);
    lv_obj_set_width(ui_LabelPin1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin1, "1");
    lv_obj_set_style_text_color(ui_LabelPin1, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin1, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin1, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin2 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin2, 80);
    lv_obj_set_height(ui_ButtonPin2, 80);
    lv_obj_set_x(ui_ButtonPin2, 0);
    lv_obj_set_y(ui_ButtonPin2, -80);
    lv_obj_set_align(ui_ButtonPin2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin2, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin2, 255, LV_STATE_ANY);

    ui_LabelPin2 = lv_label_create(ui_ButtonPin2);
    lv_obj_set_width(ui_LabelPin2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin2, "2");
    lv_obj_set_style_text_color(ui_LabelPin2, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin2, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin2, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin3 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin3, 80);
    lv_obj_set_height(ui_ButtonPin3, 80);
    lv_obj_set_x(ui_ButtonPin3, 90);
    lv_obj_set_y(ui_ButtonPin3, -80);
    lv_obj_set_align(ui_ButtonPin3, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin3, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin3, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin3, 255, LV_STATE_ANY);

    ui_LabelPin3 = lv_label_create(ui_ButtonPin3);
    lv_obj_set_width(ui_LabelPin3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin3, "3");
    lv_obj_set_style_text_color(ui_LabelPin3, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin3, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin3, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin4 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin4, 80);
    lv_obj_set_height(ui_ButtonPin4, 80);
    lv_obj_set_x(ui_ButtonPin4, -90);
    lv_obj_set_y(ui_ButtonPin4, 10);
    lv_obj_set_align(ui_ButtonPin4, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin4, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin4, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin4, 255, LV_STATE_ANY);

    ui_LabelPin4 = lv_label_create(ui_ButtonPin4);
    lv_obj_set_width(ui_LabelPin4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin4, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin4, "4");
    lv_obj_set_style_text_color(ui_LabelPin4, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin4, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin4, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin5 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin5, 80);
    lv_obj_set_height(ui_ButtonPin5, 80);
    lv_obj_set_x(ui_ButtonPin5, 0);
    lv_obj_set_y(ui_ButtonPin5, 10);
    lv_obj_set_align(ui_ButtonPin5, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin5, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin5, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin5, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin5, 255, LV_STATE_ANY);

    ui_LabelPin5 = lv_label_create(ui_ButtonPin5);
    lv_obj_set_width(ui_LabelPin5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin5, "5");
    lv_obj_set_style_text_color(ui_LabelPin5, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin5, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin5, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin6 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin6, 80);
    lv_obj_set_height(ui_ButtonPin6, 80);
    lv_obj_set_x(ui_ButtonPin6, 90);
    lv_obj_set_y(ui_ButtonPin6, 10);
    lv_obj_set_align(ui_ButtonPin6, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin6, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin6, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin6, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin6, 255, LV_STATE_ANY);

    ui_LabelPin6 = lv_label_create(ui_ButtonPin6);
    lv_obj_set_width(ui_LabelPin6, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin6, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin6, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin6, "6");
    lv_obj_set_style_text_color(ui_LabelPin6, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin6, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin6, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin7 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin7, 80);
    lv_obj_set_height(ui_ButtonPin7, 80);
    lv_obj_set_x(ui_ButtonPin7, -90);
    lv_obj_set_y(ui_ButtonPin7, 100);
    lv_obj_set_align(ui_ButtonPin7, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin7, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin7, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin7, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin7, 255, LV_STATE_ANY);

    ui_LabelPin7 = lv_label_create(ui_ButtonPin7);
    lv_obj_set_width(ui_LabelPin7, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin7, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin7, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin7, "7");
    lv_obj_set_style_text_color(ui_LabelPin7, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin7, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin7, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin8 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin8, 80);
    lv_obj_set_height(ui_ButtonPin8, 80);
    lv_obj_set_x(ui_ButtonPin8, 0);
    lv_obj_set_y(ui_ButtonPin8, 100);
    lv_obj_set_align(ui_ButtonPin8, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin8, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin8, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin8, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin8, 255, LV_STATE_ANY);

    ui_LabelPin8 = lv_label_create(ui_ButtonPin8);
    lv_obj_set_width(ui_LabelPin8, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin8, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin8, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin8, "8");
    lv_obj_set_style_text_color(ui_LabelPin8, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin8, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin8, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin9 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin9, 80);
    lv_obj_set_height(ui_ButtonPin9, 80);
    lv_obj_set_x(ui_ButtonPin9, 90);
    lv_obj_set_y(ui_ButtonPin9, 100);
    lv_obj_set_align(ui_ButtonPin9, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin9, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin9, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin9, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin9, 255, LV_STATE_ANY);

    ui_LabelPin9 = lv_label_create(ui_ButtonPin9);
    lv_obj_set_width(ui_LabelPin9, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin9, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin9, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin9, "9");
    lv_obj_set_style_text_color(ui_LabelPin9, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin9, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin9, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPinCancel = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPinCancel, 80);
    lv_obj_set_height(ui_ButtonPinCancel, 80);
    lv_obj_set_x(ui_ButtonPinCancel, -90);
    lv_obj_set_y(ui_ButtonPinCancel, 190);
    lv_obj_set_align(ui_ButtonPinCancel, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPinCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPinCancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPinCancel, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPinCancel, 255, LV_STATE_ANY);

    ui_LabelPINCancel = lv_label_create(ui_ButtonPinCancel);
    lv_obj_set_width(ui_LabelPINCancel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPINCancel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPINCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPINCancel, "C");
    lv_obj_set_style_text_color(ui_LabelPINCancel, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPINCancel, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPINCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPin0 = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPin0, 80);
    lv_obj_set_height(ui_ButtonPin0, 80);
    lv_obj_set_x(ui_ButtonPin0, 0);
    lv_obj_set_y(ui_ButtonPin0, 190);
    lv_obj_set_align(ui_ButtonPin0, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPin0, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPin0, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPin0, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPin0, 255, LV_STATE_ANY);

    ui_LabelPin0 = lv_label_create(ui_ButtonPin0);
    lv_obj_set_width(ui_LabelPin0, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPin0, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPin0, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPin0, "0");
    lv_obj_set_style_text_color(ui_LabelPin0, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPin0, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPin0, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPinOK = lv_btn_create(ui_ScreenPin);
    lv_obj_set_width(ui_ButtonPinOK, 80);
    lv_obj_set_height(ui_ButtonPinOK, 80);
    lv_obj_set_x(ui_ButtonPinOK, 90);
    lv_obj_set_y(ui_ButtonPinOK, 190);
    lv_obj_set_align(ui_ButtonPinOK, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPinOK, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPinOK, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPinOK, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPinOK, 255, LV_STATE_ANY);

    ui_LabelPinOK = lv_label_create(ui_ButtonPinOK);
    lv_obj_set_width(ui_LabelPinOK, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPinOK, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPinOK, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPinOK, "OK");
    lv_obj_set_style_text_color(ui_LabelPinOK, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPinOK, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPinOK, &ui_font_UbuntuBoldItalic, 0);


    lv_obj_add_event_cb(ui_ButtonPin0, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(ui_ButtonPin1, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(ui_ButtonPin2, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)2);
    lv_obj_add_event_cb(ui_ButtonPin3, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)3);
    lv_obj_add_event_cb(ui_ButtonPin4, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)4);
    lv_obj_add_event_cb(ui_ButtonPin5, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)5);
    lv_obj_add_event_cb(ui_ButtonPin6, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)6);
    lv_obj_add_event_cb(ui_ButtonPin7, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)7);
    lv_obj_add_event_cb(ui_ButtonPin8, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)8);
    lv_obj_add_event_cb(ui_ButtonPin9, ui_event_ButtonPinNumberClicked, LV_EVENT_CLICKED, (void *)9);
    lv_obj_add_event_cb(ui_ButtonPinCancel, ui_event_ButtonPinCancelClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPinOK, ui_event_ButtonPinOKClicked, LV_EVENT_CLICKED, NULL);

}

void ui_ScreenAbout_screen_init(void)
{
    ui_ScreenAbout = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenAbout, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenAbout, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenAbout, 255, LV_STATE_ANY);

    ui_PanelAboutHeader = lv_obj_create(ui_ScreenAbout);
    lv_obj_set_width(ui_PanelAboutHeader, gfx->width());
    lv_obj_set_height(ui_PanelAboutHeader, 50);
    lv_obj_clear_flag(ui_PanelAboutHeader, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_PanelAboutHeader, 0, LV_STATE_ANY);
    lv_obj_set_style_bg_color(ui_PanelAboutHeader, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelAboutHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelAboutHeader, 0, LV_STATE_ANY);

    ui_LabelAboutHeader = lv_label_create(ui_PanelAboutHeader);
    lv_obj_set_width(ui_LabelAboutHeader, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutHeader, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAboutHeader, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutHeader, BB_NAME);
    lv_obj_set_style_text_color(ui_LabelAboutHeader, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAboutHeader, &ui_font_UbuntuBoldItalic, 0);

    ui_ImageAbout = lv_img_create(ui_ScreenAbout);
#ifdef BRANDING_BITCOIN_FRIESLAND
    LV_IMG_DECLARE(Bitcoin_Friesland);
    lv_img_set_src(ui_ImageAbout, &Bitcoin_Friesland);
#elif BRANDING_WHY2025
    LV_IMG_DECLARE(WHY2025_logo);
    lv_img_set_src(ui_ImageAbout, &WHY2025_logo);
#elif BRANDING_BITCOINTAPS
    LV_IMG_DECLARE(BitcoinTaps_vkWhite256);
    lv_img_set_src(ui_ImageAbout, &BitcoinTaps_vkWhite256);
#else    
    LV_IMG_DECLARE(kanhetal256);
    lv_img_set_src(ui_ImageAbout, &kanhetal256); 
#endif

    lv_obj_set_width(ui_ImageAbout, LV_SIZE_CONTENT);   /// 256
    lv_obj_set_height(ui_ImageAbout, LV_SIZE_CONTENT);    /// 256
    lv_obj_set_y(ui_ImageAbout, -50);
    lv_obj_set_align(ui_ImageAbout, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImageAbout, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_ImageAbout, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_LabelAboutStatus = lv_label_create(ui_ScreenAbout);
    lv_obj_set_width(ui_LabelAboutStatus, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutStatus, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAboutStatus, 0);
    lv_obj_set_y(ui_LabelAboutStatus, -20);
    lv_obj_set_align(ui_LabelAboutStatus, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_LabelAboutStatus, "");
    lv_obj_set_style_text_color(ui_LabelAboutStatus, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutStatus, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelAboutStatus, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_ButtonAboutOne = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutOne, 100);
    lv_obj_set_height(ui_ButtonAboutOne, 100);
    lv_obj_set_align(ui_ButtonAboutOne, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_x(ui_ButtonAboutOne, 0);
    lv_obj_set_y(ui_ButtonAboutOne, -50);
    lv_obj_add_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAboutOne, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutOne, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAboutOne, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_ButtonAboutOne,LV_OBJ_FLAG_HIDDEN);
    
    ui_LabelAboutOne = lv_label_create(ui_ButtonAboutOne);
    lv_obj_set_width(ui_LabelAboutOne, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutOne, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAboutOne, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutOne, "");
    lv_obj_set_style_text_color(ui_LabelAboutOne, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutOne, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAboutOne, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonAboutTwo = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutTwo, 100);
    lv_obj_set_height(ui_ButtonAboutTwo, 100);
    lv_obj_set_align(ui_ButtonAboutTwo, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_x(ui_ButtonAboutTwo, 0);
    lv_obj_set_y(ui_ButtonAboutTwo, -50);
    lv_obj_add_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAboutTwo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutTwo, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAboutTwo, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_ButtonAboutTwo,LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutTwo = lv_label_create(ui_ButtonAboutTwo);
    lv_obj_set_width(ui_LabelAboutTwo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutTwo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAboutTwo, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutTwo, "");
    lv_obj_set_style_text_color(ui_LabelAboutTwo, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutTwo, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAboutTwo, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonAboutThree = lv_btn_create(ui_ScreenAbout);
    lv_obj_set_width(ui_ButtonAboutThree, 100);
    lv_obj_set_height(ui_ButtonAboutThree, 100);
    lv_obj_set_align(ui_ButtonAboutThree, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_x(ui_ButtonAboutThree, 0);
    lv_obj_set_y(ui_ButtonAboutThree, -50);
    lv_obj_add_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonAboutThree, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonAboutThree, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonAboutThree, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_ButtonAboutThree,LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutThree = lv_label_create(ui_ButtonAboutThree);
    lv_obj_set_width(ui_LabelAboutThree, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutThree, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAboutThree, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutThree, "");
    lv_obj_set_style_text_color(ui_LabelAboutThree, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutThree, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAboutThree, &ui_font_UbuntuBoldItalic, 0);


    ui_PanelAboutMessage = lv_obj_create(ui_ScreenAbout);
    lv_obj_set_width(ui_PanelAboutMessage, 240);
    lv_obj_set_height(ui_PanelAboutMessage, 100);
    lv_obj_clear_flag(ui_PanelAboutMessage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_align(ui_PanelAboutMessage, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_PanelAboutMessage, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_border_color(ui_PanelAboutMessage, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelAboutMessage, 3, LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelAboutMessage, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_PanelAboutMessage,LV_OBJ_FLAG_HIDDEN);

    ui_LabelAboutMessage = lv_label_create(ui_PanelAboutMessage);
    lv_obj_set_width(ui_LabelAboutMessage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAboutMessage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAboutMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAboutMessage, "CREATING INVOICE");
    lv_obj_set_style_text_color(ui_LabelAboutMessage, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelAboutMessage, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelAboutMessage, &ui_font_UbuntuBoldItalic, 0);


    lv_obj_add_event_cb(ui_PanelAboutHeader, ui_event_PanelAboutHeaderClicked, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ButtonAboutOne, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)0);
    lv_obj_add_event_cb(ui_ButtonAboutTwo, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(ui_ButtonAboutThree, ui_event_ButtonAboutClicked, LV_EVENT_CLICKED, (void *)2);


}


void ui_ScreenPayment_screen_init(void)
{
    ui_ScreenPayment = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPayment, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenPayment, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenPayment, 255, LV_STATE_ANY);

    ui_PanelPaymentHeader = lv_obj_create(ui_ScreenPayment);
    lv_obj_set_width(ui_PanelPaymentHeader, 320);
    lv_obj_set_height(ui_PanelPaymentHeader, 50);
    lv_obj_clear_flag(ui_PanelPaymentHeader, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_PanelPaymentHeader, 0, LV_STATE_ANY);
    lv_obj_set_style_bg_color(ui_PanelPaymentHeader, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelPaymentHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelPaymentHeader, 0, LV_STATE_ANY);

    ui_LabelPaymentHeader = lv_label_create(ui_PanelPaymentHeader);
    lv_obj_set_width(ui_LabelPaymentHeader, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPaymentHeader, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPaymentHeader, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentHeader, "BitcoinTaps");
    lv_obj_set_style_text_color(ui_LabelPaymentHeader, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPaymentHeader, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPaymentHeader, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPaymentCancel = lv_btn_create(ui_ScreenPayment);
    lv_obj_set_width(ui_ButtonPaymentCancel, 120);
    lv_obj_set_height(ui_ButtonPaymentCancel, 60);
    lv_obj_set_x(ui_ButtonPaymentCancel, 0);
    lv_obj_set_y(ui_ButtonPaymentCancel, -10);
    lv_obj_set_align(ui_ButtonPaymentCancel, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonPaymentCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPaymentCancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPaymentCancel, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPaymentCancel, 255, LV_STATE_ANY);

    ui_LabelPaymentCancel = lv_label_create(ui_ButtonPaymentCancel);
    lv_obj_set_width(ui_LabelPaymentCancel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPaymentCancel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPaymentCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentCancel, "CANCEL");
    lv_obj_set_style_text_color(ui_LabelPaymentCancel, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPaymentCancel, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPaymentCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonPaymentEnterPin = lv_btn_create(ui_ScreenPayment);
    lv_obj_set_width(ui_ButtonPaymentEnterPin, 120);
    lv_obj_set_height(ui_ButtonPaymentEnterPin, 60);
    lv_obj_set_x(ui_ButtonPaymentEnterPin, -70);
    lv_obj_set_y(ui_ButtonPaymentEnterPin, -10);
    lv_obj_set_align(ui_ButtonPaymentEnterPin, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonPaymentEnterPin, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPaymentEnterPin, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPaymentEnterPin, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPaymentEnterPin, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_ButtonPaymentEnterPin,LV_OBJ_FLAG_HIDDEN);

    ui_LabelPaymentEnterPin = lv_label_create(ui_ButtonPaymentEnterPin);
    lv_obj_set_width(ui_LabelPaymentEnterPin, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPaymentEnterPin, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPaymentEnterPin, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentEnterPin, "ENTER PIN");
    lv_obj_set_style_text_color(ui_LabelPaymentEnterPin, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPaymentEnterPin, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPaymentEnterPin, &ui_font_UbuntuBoldItalic, 0);

    // initialize the QR code
    lv_color_t bg_color = lv_color_hex(0x000000);
    lv_color_t fg_color = lv_color_hex(0xFFFFFF);
    ui_QrcodePayment = lv_qrcode_create(ui_ScreenPayment);
    lv_qrcode_set_size(ui_QrcodePayment,240);
    lv_qrcode_set_light_color(ui_QrcodePayment,fg_color);
    lv_qrcode_set_dark_color(ui_QrcodePayment,bg_color);
    lv_obj_center(ui_QrcodePayment);
    lv_obj_set_pos(ui_QrcodePayment,0, -30);
    lv_obj_set_style_border_width(ui_QrcodePayment, 3, LV_STATE_ANY);
    lv_obj_set_style_border_color(ui_QrcodePayment, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_add_flag(ui_QrcodePayment, LV_OBJ_FLAG_HIDDEN);

    ui_PanelPaymentMessage = lv_obj_create(ui_ScreenPayment);
    lv_obj_set_y(ui_PanelPaymentMessage,140);
    lv_obj_set_width(ui_PanelPaymentMessage, 256);
    lv_obj_set_height(ui_PanelPaymentMessage, 30);
    lv_obj_clear_flag(ui_PanelPaymentMessage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_align(ui_PanelPaymentMessage, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_PanelPaymentMessage, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_border_color(ui_PanelPaymentMessage, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelPaymentMessage, 3, LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelPaymentMessage, 255, LV_STATE_ANY);
     lv_obj_add_flag(ui_PanelPaymentMessage,LV_OBJ_FLAG_HIDDEN);

    ui_LabelPaymentMessage = lv_label_create(ui_PanelPaymentMessage);
    lv_obj_set_width(ui_LabelPaymentMessage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelPaymentMessage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelPaymentMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelPaymentMessage, "THIS IS A TEST");
    lv_obj_set_style_text_color(ui_LabelPaymentMessage, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelPaymentMessage, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelPaymentMessage, &ui_font_UbuntuBoldItalic, 0);


    lv_obj_add_event_cb(ui_ButtonPaymentEnterPin,ui_event_ButtonPaymentEnterPinClicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ButtonPaymentCancel, ui_event_ButtonPaymentCancelClicked, LV_EVENT_CLICKED, NULL);

}

void ui_ScreenConfig_screen_init(void)
{
    ui_ScreenConfig = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenConfig, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScreenConfig, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ScreenConfig, 255, LV_STATE_ANY);

    ui_LabelConfigSSID = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigSSID, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigSSID, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigSSID, 10);
    lv_obj_set_y(ui_LabelConfigSSID, 15);
    lv_label_set_text(ui_LabelConfigSSID, "SSID");
    lv_obj_set_style_text_color(ui_LabelConfigSSID, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigSSID, 255, LV_STATE_ANY);
//    lv_obj_set_style_text_font(ui_LabelConfigSSID, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigSSID = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigSSID, 240);
    lv_obj_set_height(ui_TextAreaConfigSSID, LV_SIZE_CONTENT);    /// 50
    lv_obj_set_align(ui_TextAreaConfigSSID, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(ui_TextAreaConfigSSID, -10);
    lv_obj_set_y(ui_TextAreaConfigSSID, 5);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigSSID, "Wi-Fi SSID");
    lv_textarea_set_one_line(ui_TextAreaConfigSSID, true);

    ui_LabelConfigWifiPassword = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigWifiPassword, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigWifiPassword, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigWifiPassword, 10);
    lv_obj_set_y(ui_LabelConfigWifiPassword, 60);
    lv_label_set_text(ui_LabelConfigWifiPassword, "Secret");
    lv_obj_set_style_text_color(ui_LabelConfigWifiPassword, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigWifiPassword, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelConfigWifiPassword, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigWifiPassword = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigWifiPassword, 240);
    lv_obj_set_height(ui_TextAreaConfigWifiPassword, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_align(ui_TextAreaConfigWifiPassword, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_x(ui_TextAreaConfigWifiPassword, -10);
    lv_obj_set_y(ui_TextAreaConfigWifiPassword, 50);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigWifiPassword, "Wi-Fi password");
    lv_textarea_set_one_line(ui_TextAreaConfigWifiPassword, true);
    //lv_textarea_set_password_mode(ui_TextAreaConfigWifiPassword, true);

    ui_LabelConfigLNbitsHost = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigLNbitsHost, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigLNbitsHost, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigLNbitsHost, 10);
    lv_obj_set_y(ui_LabelConfigLNbitsHost, 105);
    lv_label_set_text(ui_LabelConfigLNbitsHost, "LNbits host");
    lv_obj_set_style_text_color(ui_LabelConfigLNbitsHost, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigLNbitsHost, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelConfigLNbitsHost, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigLNbitsHost = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigLNbitsHost, 200);
    lv_obj_set_height(ui_TextAreaConfigLNbitsHost, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigLNbitsHost, -10);
    lv_obj_set_y(ui_TextAreaConfigLNbitsHost, 95);
    lv_obj_set_align(ui_TextAreaConfigLNbitsHost, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigLNbitsHost, "LNbits host");
    lv_textarea_set_one_line(ui_TextAreaConfigLNbitsHost, true);

    ui_LabelConfigDeviceID = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigDeviceID, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigDeviceID, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigDeviceID, 10);
    lv_obj_set_y(ui_LabelConfigDeviceID, 150);
    lv_label_set_text(ui_LabelConfigDeviceID, "Device ID");
    lv_obj_set_style_text_color(ui_LabelConfigDeviceID, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigDeviceID, 255, LV_STATE_ANY);
//    lv_obj_set_style_text_font(ui_LabelConfigDeviceID, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigDeviceID = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigDeviceID, 200);
    lv_obj_set_height(ui_TextAreaConfigDeviceID, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigDeviceID, -10);
    lv_obj_set_y(ui_TextAreaConfigDeviceID, 140);
    lv_obj_set_align(ui_TextAreaConfigDeviceID, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigDeviceID, "Device ID");
    lv_textarea_set_one_line(ui_TextAreaConfigDeviceID, true);

    ui_LabelConfigServo = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigServo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigServo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigServo, 10);
    lv_obj_set_y(ui_LabelConfigServo, 195);
    lv_label_set_text(ui_LabelConfigServo, "Servo");
    lv_obj_set_style_text_color(ui_LabelConfigServo, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigServo, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelConfigServo, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigServoClosed = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigServoClosed, 95);
    lv_obj_set_height(ui_TextAreaConfigServoClosed, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigServoClosed, 0);
    lv_obj_set_y(ui_TextAreaConfigServoClosed, 185);
    lv_obj_set_align(ui_TextAreaConfigServoClosed, LV_ALIGN_TOP_MID);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigServoClosed, "--");
    lv_textarea_set_one_line(ui_TextAreaConfigServoClosed, true);

    ui_TextAreaConfigServoOpen = lv_textarea_create(ui_ScreenConfig);
    lv_obj_set_width(ui_TextAreaConfigServoOpen, 95);
    lv_obj_set_height(ui_TextAreaConfigServoOpen, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigServoOpen, -10);
    lv_obj_set_y(ui_TextAreaConfigServoOpen, 185);
    lv_obj_set_align(ui_TextAreaConfigServoOpen, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigServoOpen, "--");
    lv_textarea_set_one_line(ui_TextAreaConfigServoOpen, true);

    ui_ButtonPIN = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonPIN, 95);
    lv_obj_set_height(ui_ButtonPIN, 50);
    lv_obj_set_x(ui_ButtonPIN, 10);
    lv_obj_set_y(ui_ButtonPIN, 230);
    lv_obj_set_align(ui_ButtonPIN, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_ButtonPIN, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPIN, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonPIN, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonPIN, 255, LV_STATE_ANY);

    ui_LabelConfigPIN = lv_label_create(ui_ButtonPIN);
    lv_obj_set_width(ui_LabelConfigPIN, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPIN, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigPIN, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigPIN, "PIN");
    lv_obj_set_style_text_color(ui_LabelConfigPIN, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigPIN, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigPIN, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigServoClose = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigServoClose, 95);
    lv_obj_set_height(ui_ButtonConfigServoClose, 50);
    lv_obj_set_x(ui_ButtonConfigServoClose, 0);
    lv_obj_set_y(ui_ButtonConfigServoClose, 230);
    lv_obj_set_align(ui_ButtonConfigServoClose, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_ButtonConfigServoClose, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigServoClose, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigServoClose, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigServoClose, 255, LV_STATE_ANY);

    ui_LabelConfigServoClose = lv_label_create(ui_ButtonConfigServoClose);
    lv_obj_set_width(ui_LabelConfigServoClose, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigServoClose, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigServoClose, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigServoClose, "CLOSE");
    lv_obj_set_style_text_color(ui_LabelConfigServoClose, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigServoClose, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigServoClose, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigServoOpen = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigServoOpen, 95);
    lv_obj_set_height(ui_ButtonConfigServoOpen, 50);
    lv_obj_set_x(ui_ButtonConfigServoOpen, -10);
    lv_obj_set_y(ui_ButtonConfigServoOpen, 230);
    lv_obj_set_align(ui_ButtonConfigServoOpen, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigServoOpen, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigServoOpen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigServoOpen, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigServoOpen, 255, LV_STATE_ANY);

    ui_LabelConfigServoOpen = lv_label_create(ui_ButtonConfigServoOpen);
    lv_obj_set_width(ui_LabelConfigServoOpen, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigServoOpen, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigServoOpen, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigServoOpen, "OPEN");
    lv_obj_set_style_text_color(ui_LabelConfigServoOpen, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigServoOpen, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigServoOpen, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelConfigPaymentMode = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigPaymentMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPaymentMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigPaymentMode, 10);
    lv_obj_set_y(ui_LabelConfigPaymentMode, 345);
    lv_label_set_text(ui_LabelConfigPaymentMode, "Payment mode");
    lv_obj_set_style_text_color(ui_LabelConfigPaymentMode, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigPaymentMode, 255, LV_STATE_ANY);
    lv_obj_add_flag(ui_LabelConfigPaymentMode,LV_OBJ_FLAG_HIDDEN);
    // lv_obj_set_style_text_font(ui_LabelConfigPaymentMode, &lv_font_montserrat_16, LV_STATE_ANY);

    // Drop Down for Payment mode
    ui_DropdownConfigPaymentMode = lv_dropdown_create(ui_ScreenConfig);
    lv_dropdown_set_options(ui_DropdownConfigPaymentMode, "Online\nOffline\nAuto");
    lv_obj_set_width(ui_DropdownConfigPaymentMode, 170);
    lv_obj_set_height(ui_DropdownConfigPaymentMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_DropdownConfigPaymentMode, -10);
    lv_obj_set_y(ui_DropdownConfigPaymentMode, 335);
    lv_obj_set_align(ui_DropdownConfigPaymentMode, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_DropdownConfigPaymentMode, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_add_flag(ui_DropdownConfigPaymentMode,LV_OBJ_FLAG_HIDDEN);

    // Label for control mode
    ui_LabelConfigControlMode = lv_label_create(ui_ScreenConfig);
    lv_obj_set_width(ui_LabelConfigControlMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigControlMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigControlMode, 10);
    lv_obj_set_y(ui_LabelConfigControlMode, 390);
    lv_label_set_text(ui_LabelConfigControlMode, "Control mode");
    lv_obj_set_style_text_color(ui_LabelConfigControlMode, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigControlMode, 255, LV_STATE_ANY);
    // lv_obj_set_style_text_font(ui_LabelConfigControlMode, &lv_font_montserrat_16, LV_STATE_ANY);

    // Drop Down for switching mode
    ui_DropdownConfigControlMode = lv_dropdown_create(ui_ScreenConfig);
    lv_dropdown_set_options(ui_DropdownConfigControlMode, "None\nTime\nCredit");
    lv_obj_set_width(ui_DropdownConfigControlMode, 170);
    lv_obj_set_height(ui_DropdownConfigControlMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_DropdownConfigControlMode, -10);
    lv_obj_set_y(ui_DropdownConfigControlMode, 380);
    lv_obj_set_align(ui_DropdownConfigControlMode, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_DropdownConfigControlMode, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags

    ui_ButtonConfigSave = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigSave, 95);
    lv_obj_set_height(ui_ButtonConfigSave, 50);
    lv_obj_set_x(ui_ButtonConfigSave, -10);
    lv_obj_set_y(ui_ButtonConfigSave, -5);
    lv_obj_set_align(ui_ButtonConfigSave, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigSave, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigSave, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigSave, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigSave, 255, LV_STATE_ANY);

    ui_LabelConfigDone = lv_label_create(ui_ButtonConfigSave);
    lv_obj_set_width(ui_LabelConfigDone, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigDone, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigDone, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigDone, "SAVE");
    lv_obj_set_style_text_color(ui_LabelConfigDone, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigDone, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigDone, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigCancel = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigCancel, 95);
    lv_obj_set_height(ui_ButtonConfigCancel, 50);
    lv_obj_set_x(ui_ButtonConfigCancel, 10);
    lv_obj_set_y(ui_ButtonConfigCancel, -5);
    lv_obj_set_align(ui_ButtonConfigCancel, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonConfigCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigCancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigCancel, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigCancel, 255, LV_STATE_ANY);

    ui_LabelConfigCancel = lv_label_create(ui_ButtonConfigCancel);
    lv_obj_set_width(ui_LabelConfigCancel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigCancel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigCancel, "CANCEL");
    lv_obj_set_style_text_color(ui_LabelConfigCancel, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigCancel, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigUpdate = lv_btn_create(ui_ScreenConfig);
    lv_obj_set_width(ui_ButtonConfigUpdate, 95);
    lv_obj_set_height(ui_ButtonConfigUpdate, 50);
    lv_obj_set_x(ui_ButtonConfigUpdate, 0);
    lv_obj_set_y(ui_ButtonConfigUpdate, -5);
    lv_obj_set_align(ui_ButtonConfigUpdate, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_ButtonConfigUpdate, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigUpdate, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigUpdate, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigUpdate, 255, LV_STATE_ANY);
    //lv_obj_add_flag(ui_ButtonConfigUpdate,LV_OBJ_FLAG_HIDDEN); // Hide the Update button for the moment
    
    ui_LabelConfigUpdate = lv_label_create(ui_ButtonConfigUpdate);
    lv_obj_set_width(ui_LabelConfigUpdate, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigUpdate, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigUpdate, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigUpdate, "UPDATE");
    lv_obj_set_style_text_color(ui_LabelConfigUpdate, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigUpdate, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigUpdate, &ui_font_UbuntuBoldItalic, 0);

    ui_PanelConfigPIN = lv_obj_create(ui_ScreenConfig);
    lv_obj_set_width(ui_PanelConfigPIN, 256);
    lv_obj_set_height(ui_PanelConfigPIN, 256);
    lv_obj_set_x(ui_PanelConfigPIN, 0);
    lv_obj_set_y(ui_PanelConfigPIN, 70);
    lv_obj_set_align(ui_PanelConfigPIN, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_PanelConfigPIN, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelConfigPIN, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_PanelConfigPIN, 255, LV_STATE_ANY);
    lv_obj_set_style_border_color(ui_PanelConfigPIN, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_border_opa(ui_PanelConfigPIN, 255, LV_STATE_ANY);
    lv_obj_set_style_border_width(ui_PanelConfigPIN, 2, LV_STATE_ANY);

    ui_LabelConfigPINCurrent = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINCurrent, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPINCurrent, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigPINCurrent, 0);
    lv_obj_set_y(ui_LabelConfigPINCurrent, 10);
    lv_label_set_text(ui_LabelConfigPINCurrent, "Current");

    ui_LabelConfigPINNew = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINNew, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPINNew, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigPINNew, 0);
    lv_obj_set_y(ui_LabelConfigPINNew, 60);
    lv_label_set_text(ui_LabelConfigPINNew, "New");

    ui_LabelConfigPINRepeat = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINRepeat, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPINRepeat, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigPINRepeat, 0);
    lv_obj_set_y(ui_LabelConfigPINRepeat, 110);
    lv_label_set_text(ui_LabelConfigPINRepeat, "Repeat");

    ui_TextAreaConfigPINCurrent = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINCurrent, 150);
    lv_obj_set_height(ui_TextAreaConfigPINCurrent, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_align(ui_TextAreaConfigPINCurrent, LV_ALIGN_TOP_RIGHT);
    lv_textarea_set_accepted_chars(ui_TextAreaConfigPINCurrent, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINCurrent, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINCurrent, "Current PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINCurrent, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINCurrent, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINCurrent, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigPINRepeat = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINRepeat, 150);
    lv_obj_set_height(ui_TextAreaConfigPINRepeat, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigPINRepeat, 0);
    lv_obj_set_y(ui_TextAreaConfigPINRepeat, 100);
    lv_obj_set_align(ui_TextAreaConfigPINRepeat, LV_ALIGN_TOP_RIGHT);
    if("0123456789" == "") lv_textarea_set_accepted_chars(ui_TextAreaConfigPINRepeat, NULL);
    else lv_textarea_set_accepted_chars(ui_TextAreaConfigPINRepeat, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINRepeat, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINRepeat, "Confirm PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINRepeat, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINRepeat, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINRepeat, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_TextAreaConfigPINNew = lv_textarea_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_TextAreaConfigPINNew, 150);
    lv_obj_set_height(ui_TextAreaConfigPINNew, LV_SIZE_CONTENT);    /// 70
    lv_obj_set_x(ui_TextAreaConfigPINNew, 0);
    lv_obj_set_y(ui_TextAreaConfigPINNew, 50);
    lv_obj_set_align(ui_TextAreaConfigPINNew, LV_ALIGN_TOP_RIGHT);
    if("0123456789" == "") lv_textarea_set_accepted_chars(ui_TextAreaConfigPINNew, NULL);
    else lv_textarea_set_accepted_chars(ui_TextAreaConfigPINNew, "0123456789");
    lv_textarea_set_max_length(ui_TextAreaConfigPINNew, 6);
    lv_textarea_set_placeholder_text(ui_TextAreaConfigPINNew, "New PIN");
    lv_textarea_set_one_line(ui_TextAreaConfigPINNew, true);
    lv_textarea_set_password_mode(ui_TextAreaConfigPINNew, true);
    // lv_obj_set_style_text_font(ui_TextAreaConfigPINNew, &lv_font_montserrat_16, LV_STATE_ANY);

    ui_ButtonConfigPINOK = lv_btn_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_ButtonConfigPINOK, 100);
    lv_obj_set_height(ui_ButtonConfigPINOK, 50);
    lv_obj_set_align(ui_ButtonConfigPINOK, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_add_flag(ui_ButtonConfigPINOK, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigPINOK, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigPINOK, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigPINOK, 255, LV_STATE_ANY);

    ui_LabelConfigButtonPINOK = lv_label_create(ui_ButtonConfigPINOK);
    lv_obj_set_width(ui_LabelConfigButtonPINOK, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigButtonPINOK, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigButtonPINOK, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigButtonPINOK, "OK");
    lv_obj_set_style_text_color(ui_LabelConfigButtonPINOK, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigButtonPINOK, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigButtonPINOK, &ui_font_UbuntuBoldItalic, 0);

    ui_ButtonConfigPINCancel = lv_btn_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_ButtonConfigPINCancel, 100);
    lv_obj_set_height(ui_ButtonConfigPINCancel, 50);
    lv_obj_set_align(ui_ButtonConfigPINCancel, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_add_flag(ui_ButtonConfigPINCancel, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonConfigPINCancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ButtonConfigPINCancel, lv_color_hex(BB_BGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_bg_opa(ui_ButtonConfigPINCancel, 255, LV_STATE_ANY);

    ui_LabelConfigButtonPINCancel = lv_label_create(ui_ButtonConfigPINCancel);
    lv_obj_set_width(ui_LabelConfigButtonPINCancel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigButtonPINCancel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelConfigButtonPINCancel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelConfigButtonPINCancel, "CANCEL");
    lv_obj_set_style_text_color(ui_LabelConfigButtonPINCancel, lv_color_hex(BB_FGCOLOR), LV_STATE_ANY);
    lv_obj_set_style_text_opa(ui_LabelConfigButtonPINCancel, 255, LV_STATE_ANY);
    lv_obj_set_style_text_font(ui_LabelConfigButtonPINCancel, &ui_font_UbuntuBoldItalic, 0);

    ui_LabelConfigPINMessage = lv_label_create(ui_PanelConfigPIN);
    lv_obj_set_width(ui_LabelConfigPINMessage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelConfigPINMessage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelConfigPINMessage, 0);
    lv_obj_set_y(ui_LabelConfigPINMessage, -60);
    lv_obj_set_align(ui_LabelConfigPINMessage, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_LabelConfigPINMessage, "");

    ui_KeyboardConfig = lv_keyboard_create(ui_ScreenConfig);
    lv_obj_set_width(ui_KeyboardConfig, 320);
    lv_obj_set_height(ui_KeyboardConfig, 150);
    lv_obj_add_flag(ui_KeyboardConfig, LV_OBJ_FLAG_HIDDEN);     /// Flags

    // // config events
    lv_obj_add_event_cb(ui_TextAreaConfigSSID, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigWifiPassword, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigLNbitsHost, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigDeviceID, ui_event_textarea_text_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigServoClosed, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_TextAreaConfigServoOpen, ui_event_textarea_number_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_KeyboardConfig, ui_event_keyboard_config_ready, LV_EVENT_READY, NULL);

    lv_obj_add_event_cb(ui_ButtonPIN, ui_event_button_config_pin_clicked,LV_EVENT_CLICKED,NULL);
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
    lv_obj_add_event_cb(ui_ButtonConfigPINCancel, ui_event_button_config_pin_cancel_clicked,LV_EVENT_CLICKED,NULL);
    

}



// Callback so LVGL know the elapsed time
uint32_t millis_cb(void) {
    return millis();
}


// LVGL calls it when a rendered image needs to copied to the display
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    // Call it to tell LVGL everthing is ready
    lv_disp_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    if (touch.touched()) {
        // Read touched point from touch module
        touch.readData(&x, &y);
//      Serial.printf("Touch %d %d\n",x,y);

        // lv_obj_set_x(ui_Position,x);
        // lv_obj_set_y(ui_Position,y);
    
 
        // Set the coordinates
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


void ui_init(void) {
    lvgl_sem = xSemaphoreCreateMutex();

    // Display setup
    if(!gfx->begin(40000000UL)) {
        Serial.println("Failed to initialize display!");
        return;
    }
    
    int rotation = 0;
    gfx->setRotation(rotation);
    gfx->fillScreen(BLACK);
        
    // Switch backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
        
    // Touch setup
    if(!touch.begin()) {
        Serial.println("Failed to initialize touch module!");
        return;
    }
            
    touch.enOffsetCorrection(true);
    touch.setRotation(rotation);
    
    //touch.setOffsets(Touch_X_min, Touch_X_max, TFT_res_W-1, Touch_Y_min, Touch_Y_max, TFT_res_H-1);
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
    Serial.printf("width: %d, Height: %d\n",screenWidth,screenHeight);
    uint32_t bufSize = screenWidth * screenHeight / 10;
    lv_color_t *disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        Serial.println("LVGL failed to allocate display buffer!");
        return;
    }

    // Initialize the display driver
    
    lv_display_t *disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(disp,LV_DISPLAY_ROTATION_0);

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
}



void ui_loop(void *parameters) {
    ui_tapConfig.load();
    network_restart(ui_tapConfig.getWiFiSSID(),ui_tapConfig.getWiFiPWD(),ui_tapConfig.getLNbitsHost(),ui_tapConfig.getDeviceID());

    ui_servo = new Servo();
    ui_servo->attach(5);
    internal_tap_close();



    while ( 1 == 1 ) {        
        if ( xSemaphoreTake(lvgl_sem,100) == pdTRUE ) {
            lv_task_handler();
            gfx->flush();

            unsigned long currentTime = millis();

            if (( ui_message_millis != 0 ) && ( currentTime > ui_message_millis )) {
                internal_hide_payment_message();
            }

            if (currentTime - previousRunTime >= UI_TAP_PROGRESS_INTERVAL ) {
                previousRunTime = currentTime;
                internal_update_tapprogress();
            }

            xSemaphoreGive(lvgl_sem);            
        }        
        vTaskDelay(25);
    }
}
