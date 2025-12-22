#ifndef _BITCOINTAP_H_
#define _BITCOINTAP_H_

#include "productconfig.h"


void debug(const char *function,const char *format, ...);

void tone_paid();

void tone_paymentfailed();

void tone_switches();


extern ProductConfig productConfig;
extern SemaphoreHandle_t productConfigSemaphore;



#endif
