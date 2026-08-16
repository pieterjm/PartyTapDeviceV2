#ifndef _BITCOINTAP_H_
#define _BITCOINTAP_H_

#include "productconfig.h"


void debug(const char *function,const char *format, ...);

void tone_paid();

void tone_paymentfailed();

void tone_switches();


/*
 * There are three Semaphores
 * productConfigSemaphore  - for productConfiguration, received via the websocket (handled by network.cpp)
 * networkConfigSemaphore - networkConfiguration is edited in the ui, and used to configure the network task
 * paymentDataSemaphore  - paymentData is received via the network and displayed on the ui
 */
extern ProductConfig productConfig;
extern SemaphoreHandle_t productConfigSemaphore;

extern SemaphoreHandle_t paymentDataSemaphore;
extern String paymentDataProduct; 
extern float paymentDataAmount;
extern String paymentDataCurrency;

extern SemaphoreHandle_t networkConfigSemaphore;

#endif
