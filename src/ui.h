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
bool ui_init_switches(int num, const char *label[]);
bool ui_display_qrcode(const char *s,int len);
bool ui_add_tap_credit(int credit);
bool ui_payment_screen_active();



#ifdef __cplusplus
}
#endif



#endif