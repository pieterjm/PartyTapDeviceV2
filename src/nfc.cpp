#include <Arduino.h>
#include <Adafruit_PN532_NTAG424.h>

#include "nfc.h"
#include "ui.h"
#include "network.h"
#include "bitcointap.h"

TaskHandle_t nfcTask_Handle;


#define SENSACT_NFC_CB_UNAVAILABLE  0
#define SENSACT_NFC_CB_NOREAD       1
#define SENSACT_NFC_CB_INCOMPATIBLE 2
#define SENSACT_NFC_CB_NO_NTAG424   3
#define SENSACT_NFC_CB_NO_BYTES     4
#define SENSACT_NFC_CB_READ_SUCCESS 5
#define SENSACT_NFC_CB_NO_LNURLW    6
#define SENSACT_NFC_CB_NTAG424      7
#define SENSACT_NFC_CB_READING    8


TwoWire *wire;
Adafruit_PN532 *nfc = NULL;
// PN532 *pn532 = NULL;
// PN532_I2C *pn532_i2c = NULL;
// NfcAdapter *nfcAdapter;

bool pn532_writecommand(uint8_t *cmd, uint8_t cmdlen) {
    uint8_t packet[8 + cmdlen];
    uint8_t LEN = cmdlen + 1;

    packet[0] = PN532_PREAMBLE;
    packet[1] = PN532_STARTCODE1;
    packet[2] = PN532_STARTCODE2;
    packet[3] = LEN;
    packet[4] = ~LEN + 1;
    packet[5] = PN532_HOSTTOPN532;
    uint8_t sum = PN532_HOSTTOPN532;
    for (uint8_t i = 0; i < cmdlen; i++) {
      packet[6 + i] = cmd[i];
      sum += cmd[i];
    }
    uint8_t checksum = ~sum + 1;
    packet[6 + cmdlen] = checksum;
    packet[7 + cmdlen] = PN532_POSTAMBLE;



    wire->beginTransmission(0x24);
    for(uint8_t i=0;(i< 8 + cmdlen);i++) {
        wire->write(packet[i]);
    }
    int error = wire->endTransmission();
    if ( error != 0 ) {        
        return false;
    } else {
        return true;
    }
}

bool nfc_power_off() {
     uint8_t command[3];
     
     command[0] = 0x32;
     command[1] = 0x01;
     command[2] = 0x00;
    return pn532_writecommand(command,3);

}

bool nfc_power_on() {
     uint8_t command[7];
     
     command[0] = 0x02;
     command[1] = 0x02;
     command[2] = 0x00;
     command[3] = 0xD4;
     command[4] = 0x02;
     command[5] = 0x2A;
     command[6] = 0x00;
    return pn532_writecommand(command,7);
}

void nfc_loop(void *parameters) 
{ 
    int msg = 0;

    uint8_t nfcbuffer[512];
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; 
    uint8_t uidLength;
    uint8_t success = 0;
    




    while ( 1 == 1 ) {
        success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,60000);
    

        if ( ! success ) {
            debug("nfc_loop","nothing read");
            continue;
        }

        ui_queue_send_message(SHOW_MESSAGE_READING_NFC,5000);


        if ((uidLength != 7) && (uidLength != 4)) {
            ui_queue_send_message(SHOW_MESSAGE_NFC_READ_ERROR,3000);
            continue;
        }   

        if (!nfc->ntag424_isNTAG424()) {
            ui_queue_send_message(SHOW_MESSAGE_NFC_READ_ERROR,3000);
            continue;
        }

        uint8_t bytesread = nfc->ntag424_ISOReadFile(nfcbuffer,sizeof(nfcbuffer));
        nfcbuffer[bytesread] = 0;
        if (bytesread == 0 ) {
           ui_queue_send_message(SHOW_MESSAGE_NFC_READ_ERROR,3000);
            continue;
        }



        if ( strncmp("lnurlw://",(char *)(nfcbuffer),9) != 0 ) {
            ui_queue_send_message(SHOW_MESSAGE_NFC_READ_ERROR,3000);
            continue;
        }


        event_nfc_data_read(nfcbuffer, bytesread);           
    } 
}



// Initialize the NFC sensor
bool nfc_init() {
    debug("nfc_init","");

    wire = new TwoWire(1);
    wire->begin(17,18);
    nfc = new Adafruit_PN532(-1,-1,wire);
    
    // Send the command to the PN532 and check if it was successful
    if ( nfc->begin() ) {
        debug("nfc_init","Succes init PN532");
    } else {
        debug("nfc_init","Fail init PN532");
        return false;
    }
    

    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
        debug("nfc_init","No version data");
        return false;
    } else {
        debug("nfc_init","Version data %d",versiondata);
    }
  
    // if ( ! nfc->setPassiveActivationRetries(0xFF) ) {
    //     debug("nfc_init","setPassiveActivationRetries failed");
    //     return false;
    // } else {
    //     debug("nfc_init","setPassiveActivationRetries succes");        
    // }

    if ( ! nfc->SAMConfig() ) {
        debug("nfc_init","SAMConfig fail");
        return false;
    } else {
        debug("nfc_init","SAMConfig succes");    
    }

    return true;
}


void nfc_start() {
    if (xTaskCreatePinnedToCore(
            nfc_loop,        // Function to implement the task
            "loopNFC",       // Name of the task
            4096,            // Stack size in bytes
            NULL,            // Task input parameter
            10,              // Priority of the task
            &nfcTask_Handle, // Task handle.
            0                // Core where the task should run
            ) == pdTRUE) {
        debug("setup", "NFC task succesfully started");
    } else {
        debug("setup", "Failed to start NFC task");
    }
}






bool pn532_set_rfpower(int power) {
    if (power < 0) power = 0;
    if (power > 255) power = 255;

    uint8_t command[3];
    command[0] = PN532_COMMAND_RFCONFIGURATION;
    command[1] = 0x05;
    command[2] = power;

    if ( ! nfc->sendCommandCheckAck(command,3) ) {
        debug("nfc_init","failed to set power");
        return false;
    }

    // Maybe we should read a response here first
   
    return true;
}

bool nfc_set_rf_power(int power, int gain, int modulation, int miller) {
    // Ensure power is within the valid range (0x00 to 0xFF)
    if (power < 0) power = 0;
    if (power > 255) power = 255;

    // Ensure gain is within the valid range (0x00 to 0x7F)
    if (gain < 0) gain = 0;
    if (gain > 0x7F) gain = 0x7F;

    // Ensure modulation is within the valid range (0x00 to 0x03)
    if (modulation < 0) modulation = 0;
    if (modulation > 0x03) modulation = 0x03;

    // Ensure miller is within the valid range (0x00 to 0x0F)
    if (miller < 0) miller = 0;
    if (miller > 0x0F) miller = 0x0F;

    // Create the command array
        // uint8_t command[] = {0x32, 0x05, static_cast<uint8_t>(power)};
    uint8_t command[3];
    command[0] = 0x32;
    command[1] = 0x05;
    command[2] = power;

    if ( nfc->sendCommandCheckAck(command,3,1000) ) {
        debug("nfc_init","succes setting power");
    } else {
        debug("nfc_init","failed to set power");
    }


    // return false;

    // Set modulation index
    // uint8_t modulationCommand[] = {0x32, 0x02, static_cast<uint8_t>(modulation)};
    command[0] = 0x32;
    command[1] = 0x02;
    command[2] = modulation;

    if ( nfc->sendCommandCheckAck(command,3,10000) ) {
        Serial.printf("Successfully set modulation index to %d\n",modulation);
    } else {
        Serial.println("Failed to set modulation index");
    }

    // Set number of Miller coding pulses
    // uint8_t millerCommand[] = {0x32, 0x03, static_cast<uint8_t>(miller)};
    command[0] = 0x32;
    command[1] = 0x03;
    command[2] = miller;

    if ( nfc->sendCommandCheckAck(command,3,10000) ) {
        Serial.printf("Successfully set Miller coding pulses to %d\n",miller);
    } else {
        Serial.println("Failed to set Miller coding pulses");
    }

    // Set RxGain in RFCfg register
    // uint8_t rxGainCommand[] = {0x32, 0x0A, static_cast<uint8_t>(gain)};
    command[0] = 0x32;
    command[1] = 0x0A;
    command[2] = gain;

    if ( nfc->sendCommandCheckAck(command,3,10000) ) {
        Serial.printf("Successfully set RxGain to %d\n",gain);
    } else {
        Serial.println("Failed to set RxGain");
    }

    // Return true if all commands were successful, false otherwise
    debug("nfc_set_rf_power","COmpleted");
    
    return true;
}


