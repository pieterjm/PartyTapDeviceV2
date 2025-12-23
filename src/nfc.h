#ifndef _NFC_H_
#define _NFC_H_

#include <Arduino.h>

bool nfc_init();


void nfc_start();

bool nfc_set_rf_power(int power, int gain = 0x40, int modulation = 0x03, int miller = 0x0E);

bool pn532_set_rfpower(int power);

#endif
