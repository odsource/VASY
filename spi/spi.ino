#include <SPI.h>

const int chipSelectPin = 11;



boolean SSlast = LOW;         // SS last flag.
const byte led = 9;           // Slave LED digital I/O pin.
boolean ledState = HIGH;      // LED state flag.
const byte cmdBtn = 1;        // SPI cmdBtn master command code.
const byte cmdLEDState = 2;   // 
//Initialize SPI slave.
void SlaveInit(void) {
  // Initialize SPI pins.
  pinMode(SCK, INPUT);
  pinMode(MOSI, INPUT);
  pinMode(MISO, INPUT);
  pinMode(SS, INPUT);
  // Enable SPI as slave.
  SPCR = (1 << SPE);
}
// SPI Transfer.
byte SPItransfer(byte value) {
  SPDR = value;
  while(!(SPSR & (1<<SPIF)));
  delay(10);
  return SPDR;
}



void setup() {
  // Initialize serial for troubleshooting.
  Serial.begin(9600);
  // Initialize slave LED pin.
  pinMode(led, OUTPUT);
  digitalWrite(led, ledState);
  // Initialize SPI Slave.
  SlaveInit();
  Serial.println("Slave Initialized");
}

void loop() {
  // Slave Enabled?
  if (!digitalRead(SS)) {
    // Yes, first time?
    if (SSlast != LOW) {
      // Yes, take MISO pin.
      pinMode(MISO, OUTPUT);
      Serial.println("***Slave Enabled.");
      // Write -1 slave response code and receive master command code
      byte rx = SPItransfer(255);
      Serial.println("Initial -1 slave response code sent");
      Serial.println("rx:" + String(rx) + ".");
      // cmdBtn?
      if (rx == cmdBtn) {
        // Acknowledge cmdBtn.
        byte rx = SPItransfer(cmdBtn);
        Serial.println("cmdBtn Acknowledged.");
        Serial.println("rx:" + String(rx) + ".");
        // Toggle LED State
        ledState = !ledState;
        digitalWrite(led, ledState);
      }
      // cmdLEDState?
      else if (rx == cmdLEDState) {
        // Acknowledge cmdLEDState.
        byte rx = SPItransfer(cmdLEDState);
        Serial.println("cmdLEDState Acknowledged.");
        Serial.println("rx:" + String(rx) + ".");
        rx = SPItransfer(ledState);
        Serial.println("ledState:" + String(ledState) + " Sent.");
        Serial.println("rx:" + String(rx) + ".");        
      }
      else {
        // Unrecognized command.
        byte rx = SPItransfer(255);
        Serial.println("Unrecognized Command -1 slave response code sent.");
        Serial.println("rx:" + String(rx) + ".");
      }
      // Update SSlast.
      SSlast = LOW;
    }
  }
  else {
    // No, first time?
    if (SSlast != HIGH) {
      // Yes, release MISO pin.
      pinMode(MISO, INPUT);
      Serial.println("Slave Disabled.");
      // Update SSlast.
      SSlast = HIGH;
    }
  }
}
