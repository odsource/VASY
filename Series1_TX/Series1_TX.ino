/**
 * Copyright (c) 2009 Andrew Rapp. All rights reserved.
 *
 * This file is part of XBee-Arduino.
 *
 * XBee-Arduino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * XBee-Arduino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XBee-Arduino.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <XBee.h>
#include <EEPROM.h>

/*
This example is for Series 1 XBee
Sends a TX16 or TX64 request with the value of analogRead(pin5) and checks the status response for success
Note: In my testing it took about 15 seconds for the XBee to start reporting success, so I've added a startup delay
*/

XBee xbee = XBee();

unsigned long start = millis();

// allocate two bytes for to hold a 10-bit analog reading
uint8_t payload[] = { 0, 0 };

// with Series 1 you can use either 16-bit or 64-bit addressing

// 16-bit addressing: Enter address of remote XBee, typically the coordinator
Tx16Request tx = Tx16Request(0x6666, payload, sizeof(payload));

// 64-bit addressing: This is the SH + SL address of remote XBee
//XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x4008b490);
// unless you have MY on the receiving radio set to FFFF, this will be received as a RX16 packet
//Tx64Request tx = Tx64Request(addr64, payload, sizeof(payload));

TxStatusResponse txStatus = TxStatusResponse();

int pin5 = 0;

int address = 0;
int statusLed = 11;
int errorLed = 12;
int succ_cnt = 0;
double succ_rate = 0;
double error_rate = 0;
int cnt = 0;
double rssi = 0;

Rx16Response rx16 = Rx16Response();

unsigned long rtt = 0;

void flashLed(int pin, int times, int wait) {
    
    for (int i = 0; i < times; i++) {
      digitalWrite(pin, HIGH);
      delay(wait);
      digitalWrite(pin, LOW);
      
      if (i + 1 < times) {
        delay(wait);
      }
    }
}

void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  Serial1.begin(9600);
  Serial.begin(9600);
  xbee.setSerial(Serial1);
}

void loop() {
   
   // start transmitting after a startup delay.  Note: this will rollover to 0 eventually so not best way to handle
    
      // break down 10-bit reading into two bytes and place in payload
      payload[0] = 200 & 0xff;
      payload[1] = 0 & 0xff;
      unsigned long t = millis();
      xbee.send(tx);
      cnt += 1;
      
      flashLed(statusLed, 1, 200);
      delay(1000);
      // flash TX indicator
      //flashLed(statusLed, 1, 500);
    
  
    // after sending a tx request, we expect a status response
    // wait up to 5 seconds for the status response
    if (xbee.readPacket(5000)) {

        // got a response!
      //if (end_t == 0) {
        //end_t = millis() - t;
        //Serial.write(end_t);
      //}
      //Serial.send(end_t);
        // should be a znet tx status            	
    	if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
        rtt = millis() - t;
        succ_cnt += 1;
    	  xbee.getResponse().getTxStatusResponse(txStatus);
    		xbee.getResponse().getRx16Response(rx16);
    		rssi = rx16.getRssi();
        succ_rate = succ_cnt/cnt;
        error_rate = 1 - succ_rate;
        address = 0;
        EEPROM.write(address, rssi);
        address = 1;
        EEPROM.write(address, error_rate);
        address = 2;
        EEPROM.write(address, rtt);
        Serial.print(rssi);
        Serial.print(' ');
        Serial.print(error_rate);
        Serial.print(' ');
        Serial.print(rtt);
        Serial.print('\n');
        Serial.print('\r');
    	   // get the delivery status, the fifth byte
           if (txStatus.getStatus() == SUCCESS) {
            	// success.  time to celebrate
             	//flashLed(statusLed, 5, 50);
           } else {
            	// the remote XBee did not receive our packet. is it powered on?
             	//flashLed(errorLed, 3, 500);
           }
        }      
    } else if (xbee.getResponse().isError()) {
      //nss.print("Error reading packet.  Error code: ");  
      //nss.println(xbee.getResponse().getErrorCode());
      // or flash error led
      //flashLed(statusLed, 1, 5000);
    } else {
      //flashLed(statusLed, 100, 50);
      // local XBee did not provide a timely TX Status Response.  Radio is not configured properly or connected
      //flashLed(errorLed, 2, 50);
    }
    
    
}
