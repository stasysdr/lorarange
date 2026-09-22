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

#define PIN_POWER_RXEN  18
#define PIN_POWER_TXEN  19

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
const uint32_t slaveAddresses[] = {0x12345678, 0x22345678}; //, 0x33345678};
const size_t numSlaves = sizeof(slaveAddresses) / sizeof(slaveAddresses[0]);

float distances[numSlaves] = {0.0, 0.0}; 

uint16_t calibration[3][6] = {
          { 10299, 10271, 10244, 10242, 10230, 10246 },
          { 11486, 11474, 11453, 11426, 11417, 11401 },
          { 13308, 13493, 13528, 13515, 13430, 15675  }
      };

float slaveHistory[numSlaves][3] = { {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0} };
int historyIndex[numSlaves] = {0, 0};
bool historyFull[numSlaves] = {false, false};

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

  // pinMode(PIN_POWER_RXEN, OUTPUT);
  // pinMode(PIN_POWER_TXEN, OUTPUT);
  
  // digitalWrite(PIN_POWER_RXEN, HIGH); // Turn the Receiver Amplifier ON permanently
  // digitalWrite(PIN_POWER_TXEN, LOW);  // Keep Transmitter Amplifier isolated
  

  //radio.setRfSwitchPins(PIN_POWER_RXEN, PIN_POWER_TXEN);
  int packetState = radio.setPreambleLength(16);
  if (packetState != RADIOLIB_ERR_NONE) {
    Serial.print(F("Failed to set packet parameters, code: "));
    Serial.println(packetState);
  }

  int powerState = radio.setOutputPower(-3); 

  if (powerState != RADIOLIB_ERR_NONE) {
    Serial.println(F("Power setting failed!"));
  }
  //radio.setRangingRole(RADIOLIB_SX128X_RANGING_ROLE_MASTER);
}

void loop() {
  // Wake back to a standard standby mode
  //radio.standby(); 
  delay(20); 
  static int probenr = 1;

  for (size_t i = 0; i < numSlaves; i++){
    uint32_t currentSlave = slaveAddresses[i];
    Serial.print(F("[SX1280] "));
    Serial.print(probenr);
    Serial.print(F(" Ranging Slave "));
    Serial.print(i + 1);
    Serial.print(F(" (0x"));
    Serial.print(currentSlave, HEX);
    Serial.print(F(") -> "));

    neopixelWrite(8, 0, 0, 100); 
    
    // ONE stable read per slave per loop (Passing NULL as it worked originally)
    int state = radio.range(true, currentSlave, calibration);
    
    neopixelWrite(8, 0, 0, 0);   

    if (state == RADIOLIB_ERR_NONE) {
      Serial.print("OK! : ");
  
      float rawDistance = radio.getRangingResult();
      
      // Apply your custom software calibrations
      float calibratedDistance = 0.0;
      if (i == 0) {
        calibratedDistance = rawDistance;   // Slave 1 Offset
      } else if (i == 1) {
        calibratedDistance = rawDistance;   // Slave 2 Offset
      }
      //if (calibratedDistance < 0) calibratedDistance = 0.0;

      // Add the latest clean reading to our rolling memory buffer
      int idx = historyIndex[i];
      slaveHistory[i][idx] = calibratedDistance;
      historyIndex[i] = (idx + 1) % 3; // Step index forward (0, 1, 2, 0, 1...)
      if (historyIndex[i] == 0) historyFull[i] = true;

      // Compute the rolling average from the collected history
      int count = historyFull[i] ? 3 : historyIndex[i];
      float averageSum = 0;
      for (int k = 0; k < count; k++) {
        averageSum += slaveHistory[i][k];
      }
      float rollingAverage = averageSum / count;
      distances[i] = rollingAverage; // Feed this directly to your trilateration script

      // Output data points safely
      Serial.print(calibratedDistance);
      Serial.print(F(" m | (3)Avg: "));
      Serial.print(rollingAverage);
      Serial.println(F(" m"));

    } else {
      Serial.print(F("FAILED, code "));
      Serial.println(state);
    }
    
    // Generous breathing room between probing different slaves
    delay(200); 
  }

  //Serial.println(F("Loop complete. Pausing for 3 seconds...\n"));
  //Serial.flush(); 
  probenr++; 
    
  //radio.standby(); 
  delay(3000); 
}
