#ifndef _BITCOINTAP_H_
#define _BITCOINTAP_H_

#include "tapconfig.h"


TapConfig main_get_tapconfig();

void main_update_tapconfig(TapConfig cfg);

bool main_verify_pin(String entered_pin);

void main_get_invoice(int product);

void main_nfc_read_success(const uint8_t *buffer,int num);

bool main_firmware_update();

#endif
