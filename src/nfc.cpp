#include <Arduino.h>
#include <Adafruit_PN532_NTAG424.h>

#include "nfc.h"
#include "ui.h"
#include "network.h"
#include "bitcointap.h"
// Default 7-bit USER MEMORY address, not system address 0x57.
const uint8_t TAG_ADDRESS = 0x53;
// ST25DV04K/04KC: 512; ST25DV16K/16KC: 2048; ST25DV64K/64KC: 8192.
const uint16_t EEPROM_BYTES = 2048;

// Assumes writable CC/user memory, FTM mailbox disabled, and RF read access.
// Overwrites the capability container and existing NDEF message.
// Keep a phone/reader away while programming. Provide VCC and I2C pull-ups;
// keep LPD low if present. Use voltage levels appropriate for your board.

// #include <bitcoin.h>

TaskHandle_t nfcTaskHandle;


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

SemaphoreHandle_t nfcPaymentDataSemaphore;
String nfcPaymentRequest = "";


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


bool st_waitReady() {
  const uint32_t start = millis();
  do {
    wire->beginTransmission(TAG_ADDRESS);
    if (wire->endTransmission() == 0) return true;
    delay(1);
  } while ((uint32_t)(millis() - start) < 100);
  return false;
}

bool st_writeEEPROM_original(uint16_t address, const uint8_t *data, size_t length) {
  if (address > EEPROM_BYTES || length > EEPROM_BYTES - address) return false;
  while (length) {
    // Small chunks stay within each 4-byte internal EEPROM page and
    // comfortably fit the usual 32-byte Wire transmit buffer.
    size_t n = 4 - (address & 3);
    if (n > length) n = length;
    if (!st_waitReady()) return false;
    wire->beginTransmission(TAG_ADDRESS);
    wire->write((uint8_t)(address >> 8)); // 16-bit memory address, MSB first
    wire->write((uint8_t)address);
    wire->write(data, n);
    if (wire->endTransmission() != 0) return false;
    if (!st_waitReady()) return false; // Wait for EEPROM programming
    address += n;
    data += n;
    length -= n;
  }
  return true;
}

bool st_writeEEPROM(uint16_t address,
                 const uint8_t *data,
                 size_t length) {
  if (address > EEPROM_BYTES ||
      length > EEPROM_BYTES - address) {
    return false;
  }

  delay(20);

  while (length) {
    // Keep each write within one 4-byte EEPROM page.
    size_t n = 4 - (address & 3);
    if (n > length) n = length;

    wire->beginTransmission(TAG_ADDRESS);
    wire->write((uint8_t)(address >> 8));
    wire->write((uint8_t)address);
    wire->write(data, n);

    uint8_t status = wire->endTransmission(true);

    if (status != 0) {
      Serial.print("EEPROM write failed at address 0x");
      Serial.print(address, HEX);
      Serial.print(", Wire status = ");
      Serial.println(status);
      return false;
    }

    // Fixed programming delay instead of waitReady().
    delay(20);

    address += n;
    data += n;
    length -= n;
  }

  return true;
}

bool st_writeURL(const char *url) {
  if (EEPROM_BYTES != 512 && EEPROM_BYTES != 2048 &&
      EEPROM_BYTES != 8192) return false;
  if (url == nullptr) return false;
  const size_t urlLength = strlen(url);
  if (urlLength == 0 || urlLength > EEPROM_BYTES) return false;

  const uint8_t ccLength = (EEPROM_BYTES == 512) ? 4 : 8;
  const uint16_t mlen = (EEPROM_BYTES - ccLength) / 8;
  const uint32_t payloadLength = (uint32_t)urlLength + 1; // URI ID + URL
  const bool shortRecord = payloadLength <= 255;
  const uint8_t headerLength = shortRecord ? 5 : 8; // Includes URI ID
  const uint16_t recordLength = (uint16_t)(urlLength + headerLength);
  const bool extendedTLV = recordLength >= 255;
  const uint8_t tlvLength = extendedTLV ? 4 : 2;
  // Stay within the data area advertised by the CC, including terminator.
  if ((uint32_t)tlvLength + recordLength + 1 > (uint32_t)mlen * 8)
    return false; // Reject oversize URLs before changing EEPROM
  uint8_t cc[8] = {0xE1, 0x40, (uint8_t)mlen, 0x00, 0, 0, 0, 0};
  if (ccLength == 8) {
    cc[0] = 0xE2;
    cc[2] = 0x00; // Extended CC: MLEN is in bytes 6 and 7
    cc[6] = (uint8_t)(mlen >> 8);
    cc[7] = (uint8_t)mlen;
  }

  // D1: short record. C1: normal record with 32-bit big-endian payload length.
  // Type 55 ('U') = URI; URI identifier 00 = complete, uncompressed URL.
  uint8_t header[8];
  header[0] = shortRecord ? 0xD1 : 0xC1;
  header[1] = 0x01; // Type length
  if (shortRecord) {
    header[2] = (uint8_t)payloadLength;
    header[3] = 0x55;
    header[4] = 0x00;
  } else {
    header[2] = (uint8_t)(payloadLength >> 24);
    header[3] = (uint8_t)(payloadLength >> 16);
    header[4] = (uint8_t)(payloadLength >> 8);
    header[5] = (uint8_t)payloadLength;
    header[6] = 0x55;
    header[7] = 0x00;
  }
  const uint8_t emptyTLV[] = {0x03, 0x00};
  const uint8_t terminator = 0xFE;

  if (!st_writeEEPROM(0, cc, ccLength)) return false;
  // Publish length last so a partially written record isn't advertised.
  // This does not make reformatting power-failure safe.
  if (!st_writeEEPROM(ccLength, emptyTLV, sizeof(emptyTLV))) return false;
  if (extendedTLV) {
    const uint8_t lengthBytes[] = {
      (uint8_t)(recordLength >> 8), (uint8_t)recordLength
    };
    if (!st_writeEEPROM(ccLength + 2, lengthBytes, 2)) return false;
  }
  const uint16_t recordAddress = ccLength + tlvLength;
  if (!st_writeEEPROM(recordAddress, header, headerLength)) return false;
  // Stream directly from the caller's string: no full-message RAM buffer.
  if (!st_writeEEPROM(recordAddress + headerLength,
                   (const uint8_t *)url, urlLength)) return false;
  if (!st_writeEEPROM(recordAddress + recordLength, &terminator, 1)) return false;
  // Extended TLV is 03 FF length-MSB length-LSB; publish FF last.
  const uint8_t publishedLength = extendedTLV ? 0xFF : (uint8_t)recordLength;
  return st_writeEEPROM(ccLength + 1, &publishedLength, 1);
}

// void setup() {
//   Serial.begin(115200);
//   Wire.begin(); // Uses your board's default SDA/SCL pins
//   Wire.setClock(100000);
//   delay(20);

//   if (writeURL(URL)) {
//     Serial.println(F("URL written. Tap the tag with an NFC phone."));
//   } else {
//     Serial.println(F("Write failed: check URL size, wiring/address, protection, FTM."));
//   }
// }

// void loop() {
//   // Write once per reset, not continuously: EEPROM has finite endurance.
// }


void nfc_task(void *parameters) 
{ 
    int msg = 0;

    uint8_t nfcbuffer[512];
    uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; 
    uint8_t uidLength;
    uint8_t success = 0;



    while ( 1 == 1 ) {
        //to make PN532 work in IRQ mode https://forum.arduino.cc/t/use-pn532-nfc-reader-with-interrupt/967912/6
        // poll the PN532
        // if nothing detected, turn of RF
        // set a PR if present
        
        if ( xSemaphoreTake(nfcPaymentDataSemaphore, portMAX_DELAY)) {
            if ( nfcPaymentRequest.length() > 0 ) {
                if (st_writeURL(nfcPaymentRequest.c_str()) ) {
                    Serial.println(F("URL written. Tap the tag with an NFC phone."));
                }  else {
                    Serial.println(F("Write failed: check URL size, wiring/address, protection, FTM."));
                }
                nfcPaymentRequest = "";
            }
           xSemaphoreGive(nfcPaymentDataSemaphore);
        }

        nfc->setRFField(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
        nfc->setRFField(true);
        
        // change this from blocking to interrupt based handling
        success = nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,1000);
        if ( ! success ) {


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

// Returns true if the register was read successfully.
// fieldPresent is valid only when the function returns true.
bool st_field_present() {
  wire->beginTransmission(TAG_ADDRESS);
  wire->write((uint8_t)0x20);
  wire->write((uint8_t)0x02);

  if (wire->endTransmission(false) != 0)
    return false;

  if (wire->requestFrom((uint8_t)TAG_ADDRESS,
                       (size_t)1, true) != 1)
    return false;

  bool fieldPresent =  (wire->read() & 0x04) != 0;

  if (fieldPresent) {
    debug("st_field_present","field is present");
  } else {
        debug("st_field_present","no field is present");
  }
  return fieldPresent;
}

// Initialize the NFC sensor
bool nfc_task_init() {
    debug("nfc_task_init","");

    nfcPaymentDataSemaphore = xSemaphoreCreateMutex();


    wire = new TwoWire(1);
    wire->setBufferSize(128);
    wire->begin(17,18);


    if (st_writeURL("https://www.bitcointaps.com") ) {
        debug("nfc_task_init","URL written. Tap the tag with an NFC phone.");
    }  else {
        debug("nfc_task_init","Write failed: check URL size, wiring/address, protection, FTM.");
    }


    nfc = new Adafruit_PN532(-1,-1,wire);

    

    
    // Send the command to the PN532 and check if it was successful
    if ( nfc->begin() ) {
        debug("nfc_task_init","Succes init PN532");
    } else {
        debug("nfc_task_init","Fail init PN532");
        return false;
    }
    
    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
        debug("nfc_task_init","No version data");
        return false;
    } else {
        debug("nfc_task_init","Version data %d",versiondata);
    }
  
    // if ( ! nfc->setPassiveActivationRetries(0xFF) ) {
    //     debug("nfc_task_init","setPassiveActivationRetries failed");
    //     return false;
    // } else {
    //     debug("nfc_task_init","setPassiveActivationRetries succes");        
    // }

    if ( ! nfc->SAMConfig() ) {
        debug("nfc_task_init","SAMConfig fail");
        return false;
    } else {
        debug("nfc_task_init","SAMConfig succes");    
    }

    return true;
}

void nfc_set_lightning_invoice(String pr) {
    debug("nfc_set_lightning_invoice","set invoice %s",pr.c_str());
    if ( xSemaphoreTake(nfcPaymentDataSemaphore, portMAX_DELAY)) {
        // copy payment data to shared variables and release lock
        nfcPaymentRequest = "LIGHTNING:" + pr;
        xSemaphoreGive(nfcPaymentDataSemaphore);
    }
}
    
void nfc_task_start() {
    BaseType_t result = xTaskCreatePinnedToCore(
            nfc_task,        // Function to implement the task
            "loopNFC",       // Name of the task
            20000,            // Stack size in bytes
            NULL,            // Task input parameter
            10,              // Priority of the task
            &nfcTaskHandle, // Task handle.
            0                // Core where the task should run
            );
            
    if ( result == pdTRUE) {
        debug("setup", "NFC task succesfully started");
    } else {
        debug("setup", "Failed to start NFC task with result: %d",result);
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
        debug("nfc_task_init","failed to set power");
        return false;
    }

    // Maybe we should read a response here first
   
    return true;
}

