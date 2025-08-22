#include <Arduino.h>
#include <Adafruit_PN532_NTAG424.h>

#include "nfc.h"
#include "ui.h"
#include "network.h"
#include "bitcointap.h"

static SemaphoreHandle_t nfc_sem;

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
Adafruit_PN532 *pn532 = NULL;

// Initialize the NFC sensor
bool nfc_init() {
    debug("nfc_init","");
    nfc_sem = xSemaphoreCreateMutex();

    wire = new TwoWire(1);
    wire->begin(17,18);
    pn532 = new Adafruit_PN532(-1,-1,wire);
    if ( pn532->begin() ) {
        debug("nfc_init","Succes init PN532");
    } else {
        debug("nfc_init","Fail init PN532");
        return false;
    }

    uint32_t versiondata = pn532->getFirmwareVersion();
    if (!versiondata) {
        debug("nfc_init","No version data");
        return false;
    } else {
        debug("nfc_init","Version data %d",versiondata);
    }


    if ( ! pn532->SAMConfig() ) {
        debug("nfc_init","SAMConfig fail");
        return false;
    } else {
        debug("nfc_init","SAMConfig succes");
    }
        
    return true;
}





void nfc_loop(void *parameters) 
{ 
    int msg = 0;

    uint8_t nfcbuffer[512];
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; 
    uint8_t uidLength;
    uint8_t success = 0;
    

    while ( 1 == 1 ) {
        success = pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,60000);
    

        if ( ! success ) {
            debug("nfc_loop","nothing read");
            continue;
        }

        ui_set_payment_message("READING NFC",5);

        if ((uidLength != 7) && (uidLength != 4)) {
            debug("nfc_loop","uidLength incorrect");
            ui_set_payment_message("NFC READ ERROR",3);
            continue;
        }   

        if (!pn532->ntag424_isNTAG424()) {
            ui_set_payment_message("NFC READ ERROR",3);
            Serial.println("No NTAG424");
            continue;
        }

        uint8_t bytesread = pn532->ntag424_ISOReadFile(nfcbuffer,sizeof(nfcbuffer));
        nfcbuffer[bytesread] = 0;
    
        if ( bytesread == 0 ) {
            Serial.println("no bytes read");
            ui_set_payment_message("NFC READ ERROR",3);

            continue;
        }



        if ( strncmp("lnurlw://",(char *)(nfcbuffer),9) != 0 ) {
            Serial.println("this is not an LNURLW");
            ui_set_payment_message("NFC READ ERROR",3);
            continue;
        }

        Serial.printf("Read succes: %s\n",nfcbuffer);


        event_nfc_data_read(nfcbuffer, bytesread);           
    } 
}



