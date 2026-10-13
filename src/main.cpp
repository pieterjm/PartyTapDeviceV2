// Add flow meter
// Add valve

#include <LittleFS.h>
#include <BackgroundAudioWAV.h>
#include <ESP32I2SAudio.h>
#include <FreeRTOS.h>

#include "bitcointap.h"
#include "network.h"
#include "ui.h"
#include "nfc.h"
#include "pingwav.h"
#include "levelup.h"

// create source code file for network functions
// move from main to network 




#define AUDIO_I2S_PORT I2S_NUM_0
#define AUDIO_I2S_MCK_IO -1 // MCK
#define AUDIO_I2S_BCK_IO 42 // BCK
#define AUDIO_I2S_LRCK_IO 2   // LCK
#define AUDIO_I2S_DO_IO 41  // DIN


ESP32I2SAudio audio(AUDIO_I2S_BCK_IO,AUDIO_I2S_LRCK_IO,AUDIO_I2S_DO_IO,AUDIO_I2S_MCK_IO);
ROMBackgroundAudioWAV BMP(audio);


ProductConfig productConfig;
SemaphoreHandle_t productConfigSemaphore;

SemaphoreHandle_t paymentDataSemaphore;
String paymentDataProduct = "";
float paymentDataAmount = 0;
String paymentDataCurrency = "";

SemaphoreHandle_t networkConfigSemaphore;



void debug(const char *function,const char *format, ...) {
    va_list args;
    va_start(args,format);    

    Serial.printf("[%s] ",function);
    Serial.vprintf(format, args);
    Serial.printf("\n");
    va_end(args);
}

void tone_negative() {
#ifdef NOTONE
    return;
#endif
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.setGain(2);
    BMP.write(levelup_wav, sizeof(levelup_wav));
    // BMP.write(riff_wav,riff_wav_len);
}

void tone_positive() {
#ifdef NOTONE
    return;
#endif
    BMP.flush(); // Stop any existing output, reset for new file
    // BMP.write(riff_wav,riff_wav_len);
    BMP.write(levelup_wav, sizeof(levelup_wav));
}

void tone_switches() {
    tone_positive();
}

void tone_online() {
    tone_positive();
}

void tone_offline() {
    tone_negative();
}


void tone_paid() {
    tone_positive();
}

void tone_paymentfailed() {
    tone_negative();
}
  

// Arduino initial setup call
void setup() {

    esp_log_level_set("*", ESP_LOG_NONE);

    delay(5000);

    audio.setStereo(false);
    BMP.begin();


    Serial.begin(115200);
    debug("setup","Starting up");

    LittleFS.begin(true);

    productConfigSemaphore = xSemaphoreCreateMutex();
    paymentDataSemaphore = xSemaphoreCreateMutex();
    networkConfigSemaphore = xSemaphoreCreateMutex();


    
    
    // Initialise network
    network_task_init();

    // initialiase UI
    ui_task_init();

    // initialise and start NFC
    nfc_task_init();
    nfc_task_start();

    // network start
    network_task_start();

    // UI start
    ui_task_start();


}



void loop() {      
}







