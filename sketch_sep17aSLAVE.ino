/*
  RadioLib SX128x Ranging Example

  This example performs ranging exchange between two
  SX1280 LoRa radio modules. Ranging allows to measure
  distance between the modules using time-of-flight
  measurement.

  Only SX1280 and SX1282 without external RF switch support ranging!

  Note that to get accurate ranging results, calibration is needed!
  The process is described in Semtech SX1280 Application Note AN1200.29

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx128x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

// include the library
#include <SPI.h>
#include <RadioLib.h>

#define CS                    7      // SPI chip select, often shown as 'NSS'
#define DIO1                  1      // 
#define NRST                  2      // 
#define BUSY                  3     // 

// ---------- SPI / SX1280 wiring ----------
#define PIN_SCK    6      // SPI SCK
#define PIN_MISO   5      // custom MISO
#define PIN_MOSI   4      // custom MOSI
#define PIN_CS     7      // SX1280 NSS (chip select)


 // Start SPI on custom pins

// SX1280 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// NRST pin:  3
// BUSY pin:  9
SX1280 radio = new Module(CS, DIO1, NRST, BUSY, SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0));

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/
//const uint32_t slaveAddresses[3] = {0x12345678, 0x22345678, 0x33345678};

void setup() {
  setCpuFrequencyMhz(40); 
  Serial.begin(115200);

  radio.reset();
  delay(10);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);  // SPI.begin(SCK, MISO, MOSI, SS);
  
    // initialize SX1280 at 2400 MHz
  Serial.print(F("[SX1280] Initializing ... "));
  ConfigLoRa_t config;
  config.frequency = 2480;
  config.bandwidth = 1625.0;
  config.spreadingFactor = 5;
  config.codingRate = 5;
  
  int state = radio.begin(config);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // Apply Packet Parameters (16 Preamble symbols)
  int packetState = radio.setPreambleLength(16);

  if (packetState != RADIOLIB_ERR_NONE) {
    Serial.print(F("Failed to set packet parameters, code: "));
    Serial.println(packetState);
  }

  int powerState = radio.setOutputPower(-3); 
  if (powerState != RADIOLIB_ERR_NONE) {
    Serial.println(F("Power setting failed!"));
  }

}

void loop() {

  Serial.print(F("[SX1280] Slave: Listening for Master ... "));

  // Turn NeoPixel ON (RGB values: Red=0, Green=100, Blue=0)
  neopixelWrite(8, 0, 100, 0); 

  // false = Slave (Listens and automatically echoes back)
  int state = radio.range(false, 0x12345678); 

  // Turn NeoPixel OFF
  neopixelWrite(8, 0, 0, 0);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Ping received and echoed successfully!"));
  } else if (state == RADIOLIB_ERR_RANGING_TIMEOUT) {
    Serial.println(F("Listening window closed. Re-opening..."));
  } else {
    Serial.print(F("Failed, code "));
    Serial.println(state);
  }
  
  // wait for a second before ranging again
  delay(100);
}