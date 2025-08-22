#ifndef _BITCOINTAP_H_
#define _BITCOINTAP_H_

#define DEBUG

void debug(const char *function,const char *format, ...);

void tone_paid();

void tone_paymentfailed();

void tone_switches();

#endif
