#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <Arduino.h>

void main_get_invoice(int product);

void event_nfc_data_read(const uint8_t *buffer,int num);

bool main_firmware_update();

void network_restart(String wifi_ssid,String wifi_pwd,String lnbitshost, String deviceid);

void main_loop(void *parameters);

void ws_lnurlw_withdraw(const char *lnurlw);

void network_init();


#endif
