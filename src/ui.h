#ifndef _UI_H_
#define _UI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "tapconfig.h"

void ui_init(void);
void ui_display_qrcode(const char *s,int len);
void ui_hide_qrcode();
void ui_loop(void *parameters);
void ui_init_switches(int num, const char *label[]);

void ui_update_wifi_state(const char *s);
void ui_update_websocket_state(const char *s);
void ui_update_server_version(const char *s);
void ui_update_server_branding(const char *s);

bool ui_admin_screen_active();

bool ui_payment_screen_active();
void ui_set_payment_message(const char *s,int delay);
void ui_hide_payment_message();
void ui_load_payment_screen();
void ui_add_tap_credit(int credit);
void ui_hide_payment_buttons();


void ui_load_about_screen(); 

// navigate to the tap screen from where the tap can be operated
void ui_tap_load_screen();
void ui_set_tap_bar_max(int max);
void ui_set_tap_bar_value(int max);
void ui_show_tap_bar(); 
void ui_tap_finished();
bool ui_tap_screen_active();
bool ui_tap_controls_visible();
void ui_tap_show_controls();


// extern SemaphoreHandle_t lvgl_sem;


#ifdef __cplusplus
}
#endif



#endif