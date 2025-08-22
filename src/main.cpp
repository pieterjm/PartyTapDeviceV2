// Add flow meter
// Add valve
#define DEBUG

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

TaskHandle_t nfcTask_Handle;
TaskHandle_t lvglTask_Handle;
TaskHandle_t controlTask_Handle;



void debug(const char *function,const char *format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args,format);
    Serial.printf("[%s] ",function);
    Serial.vprintf(format, args);
    Serial.printf("\n");
    va_end(args);
#endif
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
    delay(5000);

    audio.setStereo(false);
    BMP.begin();


    Serial.begin(115200);
    debug("setup","Starting up");

    LittleFS.begin(true);

    
    // network init
    network_init();
    
    // Initialise UI
    ui_init();

    // Start UI thread
   if ( xTaskCreatePinnedToCore (
        ui_loop,     // Function to implement the task
        "ui_loop",   // Name of the task
        10000,       //  Stack size in bytes
        NULL,      // Task input parameter
        5,         // Priority of the task
        &lvglTask_Handle,      // Task handle.
        1          // Core where the task should run
      ) != pdPASS ) {
        debug("setup","Failed to start UI thread");
      }





    if ( xTaskCreatePinnedToCore (
        main_loop,     // Function to implement the task
        "main_loop",   // Name of the task
        10000,      // Stack size in bytes
        NULL,      // Task input parameter
        10,         // Priority of the task
        &controlTask_Handle,      // Task handle.
        0          // Core where the task should ru
     ) ) {
        debug("setup","network loop started");
    } else {
        debug("setup","Failed to start network loop");
    }


    if ( ! nfc_init() ) {
        debug("setup","NFC initi failed. not starting NFC loop");        
    } else {
        if ( xTaskCreatePinnedToCore (
            nfc_loop,     // Function to implement the task
            "loopNFC",   // Name of the task
            4096,      // Stack size in bytes
            NULL,      // Task input parameter
            10,         // Priority of the task
            &nfcTask_Handle,      // Task handle.
            0          // Core where the task should run
            ) == pdTRUE ) {
            debug("setup","NFC task succesfully started");
        } else {
            debug("setup","Failed to start NFC task");
        }
    }



    // UBaseType_t iFreeStackNum = 0;
    // while ( 1 == 1 ) {
    //     Serial.printf("Number of tasks: %d\n",uxTaskGetNumberOfTasks());
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(nfcTask_Handle);
    //     Serial.printf("NFC Stack Free: %d\n",iFreeStackNum);
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(lvglTask_Handle);
    //     Serial.printf("LVGL Stack Free: %d\n",iFreeStackNum);
    //     iFreeStackNum = uxTaskGetStackHighWaterMark(controlTask_Handle);
    //     Serial.printf("Control Stack Free: %d\n",iFreeStackNum);
    //     vTaskDelay(30000/portTICK_PERIOD_MS);
    // }    
}



void loop() {      
}







