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

//#define LOCAL_SRC 0x1111
#define LOCAL_SRC 0x6666
//#define DEST_ADDR 0x6666
#define DEST_ADDR 0x1111
#define BROADCAST 0xFFFF

const int ARRAY_SIZE = 150;
XBee xbee = XBee();
XBeeResponse response = XBeeResponse();
// create reusable response objects for responses we expect to handle 
Rx16Response rx16 = Rx16Response();
Rx64Response rx64 = Rx64Response();

struct sentMessages {
    int flag;
    int src;
    int dest;
    int id;
    int path_cost;
};
sentMessages sent[ARRAY_SIZE];

struct routingEntry {
    int dest;
    int next;
    int cost;
};
routingEntry routingTable[ARRAY_SIZE];

struct routeDiscoveryEntry {
    int src;
    int dest;
    int prev;
    int forward_cost;
    int residual_cost;
};
routeDiscoveryEntry routeDiscoveryTable[ARRAY_SIZE];

// allocate two bytes for to hold a 10-bit analog reading
uint8_t payload[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned int pl[] = {0, 0, 0, 0};

// with Series 1 you can use either 16-bit or 64-bit addressing
int address = BROADCAST;
// 16-bit addressing: Enter address of remote XBee, typically the coordinator
Tx16Request tx = Tx16Request(address, payload, sizeof(payload));

// 64-bit addressing: This is the SH + SL address of remote XBee
// XBeeAddress64 addr64 = XBeeAddress64(0xFFFFFFFF, 0xFFFFFFFF);
// unless you have MY on the receiving radio set to FFFF, this will be received as a RX16 packet
// Tx64Request tx = Tx64Request(addr64, payload, sizeof(payload));

TxStatusResponse txStatus = TxStatusResponse();

int flag = 15;
int srcAddress = LOCAL_SRC;
int dstAddress = DEST_ADDR;
int ident = 0;

int routed = 0;
int routeIndex = 0;
int previous = 0;

int ack_flag = 0;
unsigned int i = 0;
unsigned int j = 0;

int statusLed = 11;
int errorLed = 12;
int dataLed = 13;

int cnt = 0;
double rssi = 0;

int index = 0;
uint8_t option = 0;
uint8_t data[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
sentMessages deciphered;

void sendTx() {
    // break down 10-bit reading into two bytes and place in payload
    //Serial.print("SENDING AFTER RECEIVING\r\n");
    xbee.send(tx);    
    // after sending a tx request, we expect a status response
    // wait up to 5 seconds for the status response
    if (xbee.readPacket(5000)) {
        //Serial.print("SENT PACKET WAS RECEIVED\r\n");
        // got a response!
        // should be a znet tx status              
        if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
            // get the delivery status, the fifth byte
             if (txStatus.getStatus() == SUCCESS) {
                 //Serial.print("SENT PACKET WAS RECEIVED SUCCESSFUL\r\n");
                 // success.  time to celebrate
                 //flashLed(statusLed, 5, 50);
             } else {
                 //Serial.print("SENT PACKET FAILED\r\n");
                 // the remote XBee did not receive our packet. is it powered on?
                 //flashLed(errorLed, 3, 500);
             }
        }
    } else if (xbee.getResponse().isError()) {
        Serial.print("SEND ERROR RECEIVED:");
        Serial.print(xbee.getResponse().getErrorCode());
        //nss.print("Error reading packet.  Error code: ");  
        //nss.println(xbee.getResponse().getErrorCode());
        // or flash error led
    } else {
        Serial.print("TIMEOUT\r\n");
        // local XBee did not provide a timely TX Status Response.  Radio is not configured properly or connected
        //flashLed(errorLed, 2, 50);
    }
    flag = 15
}

void decipherPayload(uint8_t payl[]) {
    // Flag
      pl[0] = payl[0] & 0xFFFF;
      // Source
      uint8_t src1 = payl[1];
      uint8_t src2 = payl[2];
      pl[1] = src1;
      pl[1] = pl[1] << 8;
      pl[1] = pl[1] | src2;
      // Destination
      src1 = payl[3];
      src2 = payl[4];
      pl[2] = src1;
      pl[2] = pl[2] << 8;
      pl[2] = pl[2] | src2;
      // Identifier
      src1 = payl[5];
      src2 = payl[6];
      pl[3] = src1;
      pl[3] = pl[3] << 8;
      pl[3] = pl[3] | src2;
      // Path cost
      src1 = payl[7];
      src2 = payl[8];
      pl[4] = src1;
      pl[4] = pl[4] << 8;
      pl[4] = pl[4] | src2;
      
      deciphered.flag = pl[0];
      deciphered.src = pl[1];
      deciphered.dest = pl[2];
      deciphered.id = pl[3];
      deciphered.path_cost = pl[4];
}

void cipherPayload(int i) {
      payload[0] = sent[i].flag & 0xFF;
      payload[1] = (sent[i].src >> 8) & 0xFF;
      payload[2] = sent[i].src & 0xFF;
      payload[3] = (sent[i].dest >> 8) & 0xFF;
      payload[4] = sent[i].dest & 0xFF;
      payload[5] = (sent[i].id >> 8) & 0xFF;
      payload[6] = sent[i].id & 0xFF;
      payload[7] = (sent[i].path_cost >> 8) & 0xFF;
      payload[8] = sent[i].path_cost & 0xFF;
      /*
      Serial.print("CIPHERED PAYLOAD:\r\n");
      Serial.print(payload[0], HEX);
      Serial.print("\r\n");
      Serial.print(payload[1], HEX);
      Serial.print("\r\n");
      Serial.print(payload[2], HEX);
      Serial.print("\r\n");
      Serial.print(payload[3], HEX);
      Serial.print("\r\n");
      Serial.print(payload[4], HEX);
      Serial.print("\r\n");
      Serial.print(payload[5], HEX);
      Serial.print("\r\n");
      Serial.print(payload[6], HEX);
      Serial.print("\r\n");
      Serial.print(payload[7], HEX);
      Serial.print("\r\n");
      */
}

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

void addRoute() {
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (routingTable[i].cost == 0) {
            routingTable[i].dest = deciphered.src;
            routingTable[i].next = rx16.getRemoteAddress16();
            routingTable[i].cost = deciphered.path_cost + 1;
            break;
        }
    }
}

void updateRoutingTable() {
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (routingTable[i].dest == deciphered.src &&
            routingTable[i].cost > deciphered.path_cost + 1) {
            routingTable[i].next = previous;
            routingTable[i].cost = deciphered.path_cost + 1;
            address = routeDiscoveryTable[index].prev;
            sent[index].path_cost = deciphered.path_cost + 1;
            flag = 0;
            break;
        } else if (routingTable[i].cost == 0) {
            routingTable[i].dest = deciphered.src;
            routingTable[i].next = previous;
            routingTable[i].cost = deciphered.path_cost + 1;
            address = routeDiscoveryTable[routeIndex].prev;
            sent[index].path_cost = deciphered.path_cost + 1;
            flag = 0;
            break;
        } else {
            flag = 15
        }
    }
}

void setup() {
    pinMode(statusLed, OUTPUT);
    pinMode(errorLed, OUTPUT);
    pinMode(dataLed,  OUTPUT);

    sentMessages sm = {
        sm.flag = 15,
        sm.src = 0,
        sm.dest = 0,
        sm.id = 0,
        sm.path_cost = 0
    };
    
    routingEntry re = {
        re.dest = 0,
        re.next = 0,
        re.cost = 0
    };
    
    routeDiscoveryEntry rde = {
        rde.src = 0,
        rde.dest = 0,
        rde.prev = 0,
        rde.forward_cost = 64000,
        rde.residual_cost = 64000
    };
    
    for(int i = 0; i < ARRAY_SIZE; i++) {
        sent[i] = sm;
        routingTable[i] = re;
        routeDiscoveryTable[i] = rde;
    }
  
    // start serial
    Serial1.begin(9600);
    Serial.begin(9600);
    xbee.setSerial(Serial1);

    payload[0] = 0x00 & 0xFF;
    payload[1] = (LOCAL_SRC >> 8) & 0xFF;
    payload[2] = LOCAL_SRC & 0xFF;
    payload[3] = (DEST_ADDR >> 8) & 0xFF;
    payload[4] = DEST_ADDR & 0xFF;
    payload[5] = (0x0000 >> 8) & 0xFF;
    payload[6] = 0x0000 & 0xFF;
    payload[7] = (0x0000 >> 8) & 0xFF;
    payload[8] = 0x0000 & 0xFF;
  
    xbee.send(tx);
    // after sending a tx request, we expect a status response
    // wait up to 5 seconds for the status response
    if (xbee.readPacket(5000)) {
        Serial.print("TEST: SENT PACKET WAS RECEIVED\r\n");
        // got a response!
        // should be a znet tx status              
        if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
            // get the delivery status, the fifth byte
            if (txStatus.getStatus() == SUCCESS) {
                Serial.print("TEST: SENT PACKET WAS RECEIVED SUCCESSFUL\r\n");
                // success.  time to celebrate
                //flashLed(statusLed, 5, 50);
            } else {
                Serial.print("TEST: SENT PACKET FAILED\r\n");
                // the remote XBee did not receive our packet. is it powered on?
                //flashLed(errorLed, 3, 500);
            }
        }
    } else if (xbee.getResponse().isError()) {
        Serial.print("TEST: SEND ERROR RECEIVED:");
        Serial.print(xbee.getResponse().getErrorCode());
        //nss.print("Error reading packet.  Error code: ");  
        //nss.println(xbee.getResponse().getErrorCode());
        // or flash error led
    } else {
        Serial.print("TEST: TIMEOUT\r\n");
        // local XBee did not provide a timely TX Status Response.  Radio is not configured properly or connected
        //flashLed(errorLed, 2, 50);
    }
}

void loop() {
    /************************************************************************/
    /************************** ANFANG TEST SENDEN***************************/
    /************************************************************************/
    /*if (i > 40000) {
        i = 0;
        j += 1;
        if (j > 10) {
            j = 0;
            xbee.send(tx);
            // after sending a tx request, we expect a status response
            // wait up to 5 seconds for the status response
            if (xbee.readPacket(5000)) {
                Serial.print("TEST: SENT PACKET WAS RECEIVED\r\n");
                // got a response!
                // should be a znet tx status              
              if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
                 // get the delivery status, the fifth byte
                   if (txStatus.getStatus() == SUCCESS) {
                      Serial.print("TEST: SENT PACKET WAS RECEIVED SUCCESSFUL\r\n");
                      // success.  time to celebrate
                      //flashLed(statusLed, 5, 50);
                   } else {
                      Serial.print("TEST: SENT PACKET FAILED\r\n");
                      // the remote XBee did not receive our packet. is it powered on?
                      //flashLed(errorLed, 3, 500);
                   }
                }
            } else if (xbee.getResponse().isError()) {
              Serial.print("TEST: SEND ERROR RECEIVED:");
              Serial.print(xbee.getResponse().getErrorCode());
              //nss.print("Error reading packet.  Error code: ");  
              //nss.println(xbee.getResponse().getErrorCode());
              // or flash error led
            } else {
              Serial.print("TEST: TIMEOUT\r\n");
              // local XBee did not provide a timely TX Status Response.  Radio is not configured properly or connected
              //flashLed(errorLed, 2, 50);
            }
        }
    }
    ident += 1;
    payload[6] = ident & 0xFF;*/
    /************************************************************************/
    /**************************** ENDE TEST SENDEN***************************/
    /************************************************************************/
    xbee.readPacket();

    if (xbee.getResponse().isAvailable()) {
        // got something
        Serial.print("GOT A RESPONSE\r\n");
        if (xbee.getResponse().getApiId() == RX_16_RESPONSE || xbee.getResponse().getApiId() == RX_64_RESPONSE) {
            // got a rx packet
        
            if (xbee.getResponse().getApiId() == RX_64_RESPONSE) {
                xbee.getResponse().getRx64Response(rx64);
                Serial.print("Was soll der Scheiss\r\n");
            } else {
                Serial.print("RECEIVED RX16\r\n");
                xbee.getResponse().getRx16Response(rx16);
                //option = rx16.getOption();
                //data = rx16.getData(0);
          
                for(int i=0; i<9;i++) {
                    data[i] = rx16.getData(i);
                }
                Serial.print("DATA RECEIVED IN HEX: \r\n");
                for (int i = 0; i < 9; i++) {
                    Serial.print(data[i], HEX);
                    Serial.print("\r\n");
                }
                decipherPayload(data);
                /***********************************************************/
                /* Prüfung der empfangenen Daten einfügen */
                /***********************************************************/
                if(pl[2] == LOCAL_SRC) {
                    Serial.print("MEINS\r\n");
                    for(int i = 0; i < ARRAY_SIZE; i++) {
                        index = i;
                        // Leerer oder passender Eintrag in der RouteDiscoveryTable 
                        // RREQ Message
                        if (pl[0] == 2) {
                            if (routingTable[i].dest == deciphered.src) {
                                routed = 1;
                                routeIndex = i;
                            }
                            if ((routeDiscoveryTable[i].src == 0) &&
                                (routeDiscoveryTable[i].dest == 0)) {
                                routeDiscoveryTable[i].src = deciphered.src;
                                routeDiscoveryTable[i].dest = deciphered.dest;
                                routeDiscoveryTable[i].prev = rx16.getRemoteAddress16();
                                routeDiscoveryTable[i].forward_cost = deciphered.path_cost + 1;
                                routeDiscoveryTable[i].residual_cost = 0;
                                address = routeDiscoveryTable[i].prev;
                                Serial.print("RREQ\r\n");
                                flag = 0;
                                break;
                            } else if (((routeDiscoveryTable[i].src == deciphered.src) &&
                                (routeDiscoveryTable[i].dest == deciphered.dest))) {
                                if (routeDiscoveryTable[i].forward_cost >= deciphered.path_cost+1) {
                                    routeDiscoveryTable[i].forward_cost = deciphered.path_cost + 1;
                                    routeDiscoveryTable[i].prev = rx16.getRemoteAddress16();
                                    routeDiscoveryTable[i].residual_cost = 0;
                                    address = routeDiscoveryTable[i].prev;
                                    Serial.print("RREQ\r\n");
                                    flag = 0;
                                    break;
                                }
                            }
                        } else {
                            if(sent[i].flag == 15) {
                                Serial.print("NEW MESSAGE\r\n");
                                flag = 0;
                                break;
                            } else if((sent[i].flag == deciphered.flag) &&
                                (sent[i].src == deciphered.src) &&
                                (sent[i].dest == deciphered.dest) &&
                                (sent[i].id == deciphered.id)) {
                                // flag == 1 bedeutet, dass das empfangene Paket bereits weitergeleitet wurde
                                flag = 1;
                                Serial.print("OLD MESSAGE\r\n");
                                break;
                            }
                        }
                    }
                    // flag auf 0 setzen um ACKNOWLEDGEMENT zu senden
                    if(flag == 0) {
                        sent[index] = deciphered;
                        sent[index].flag = 1;
                        if (deciphered.flag == 2) {
                            sent[index].flag = 3;
                            sent[index].path_cost = routeDiscoveryTable[index].residual_cost;
                            address = rx16.getRemoteAddress16();
                            if (routed == 0) {
                                addRoute();
                            }
                        }
                        sent[index].src = LOCAL_SRC;
                        sent[index].dest = deciphered.src;
                        sent[index].id = ident;
                        ident += 1;
                        Serial.print("SEND ACKNOWLEDGEMENT\r\n");
                    }
                    if(deciphered.flag == 1) {
                        flag = 1;
                        ack_flag = 1;
                        Serial.print("ACKNOWLEDGEMENT RECEIVED\r\n");
                    }
                    // Type 0..3 -> 0 = Normal, 1 = ACK, 2 = RREQ, 3 = RREP
                } else if(pl[0] == 0 || pl[0] == 1 || pl[0] == 2 || pl[0] == 3) {
                    Serial.print("NICHT MEINS\r\n");
                    for(int i = 0; i < ARRAY_SIZE; i++) {
                        index = i;
                        // RREQ
                        if (deciphered.flag == 2) {
                            flag = 0;
                            break;
            
                        //RREP
                        } else if (deciphered.flag == 3) {
                            flag = 0;
                            break;
                        }
                        if(sent[i].flag == 15) {
                            flag = 0;
                            break;
                        } else if((sent[i].flag == deciphered.flag) &&
                            (sent[i].src == deciphered.src) &&
                            (sent[i].dest == deciphered.dest) &&
                            (sent[i].id == deciphered.id)){
                            // flag == 1 bedeutet, dass das empfangene Paket bereits weitergeleitet wurde
                            flag = 1;
                            break;
                        }
                    }
                    // flag == 0 bedeutet, dass wir das empfangene Paket abspeichern und weiterleiten müssen
                    if (flag == 0) {
                        sent[index] = deciphered;

                        /////////////////////////////////////////////////////////////////
                        // RREQ
                        if (pl[0] == 2) {
                            for (int i = 0; i < ARRAY_SIZE; i++) {
                                if (routingTable[i].dest == deciphered.dest) {
                                    routed = 1;
                                    routeIndex = i;
                                }
                                // Leerer Eintrag in der RouteDiscoveryTable 
                                if ((routeDiscoveryTable[i].src == 0) &&
                                    (routeDiscoveryTable[i].dest == 0)) {
                                    routeDiscoveryTable[i].src = deciphered.src;
                                    routeDiscoveryTable[i].dest = deciphered.dest;
                                    routeDiscoveryTable[i].prev = rx16.getRemoteAddress16();
                                    routeDiscoveryTable[i].forward_cost = deciphered.path_cost + 1;
                                    sent[index].flag = 2;
                                    sent[index].path_cost = routeDiscoveryTable[i].forward_cost;
                                    address = BROADCAST;
                                    /*
                                    if (routed == 1) {
                                        routeDiscoveryTable[i].residual_cost = routingTable[routeIndex].cost;
                                        address = rx16.getRemoteAddress16();
                                        sent[index].flag = 3;
                                        sent[index].path_cost = routeDiscoveryTable[i].residual_cost;
                                    }
                                    */
                                    Serial.print("RREQ\r\n");
                                    break;
                                    // Passender Eintrag in der RouteDiscoveryTable 
                                } else if (((routeDiscoveryTable[i].src == deciphered.src) &&
                                    (routeDiscoveryTable[i].dest == deciphered.dest))) {
                                    if (routeDiscoveryTable[i].forward_cost >= deciphered.path_cost+1) {
                                        routeDiscoveryTable[i].forward_cost = deciphered.path_cost + 1;
                                        routeDiscoveryTable[i].prev = rx16.getRemoteAddress16();
                                        if (routed == 1) {
                                            routeDiscoveryTable[i].residual_cost = routingTable[routeIndex].cost;
                                            address = rx16.getRemoteAddress16();
                                            sent[index].flag = 3;
                                            sent[index].path_cost = routeDiscoveryTable[i].residual_cost;
                                        } else {
                                            address = BROADCAST;
                                            sent[index].path_cost = routeDiscoveryTable[i].forward_cost;
                                        }
                                        break;
                                        Serial.print("RREQ\r\n");
                                    }
                                    flag = 15;
                                    break;
                                }
                            }
                            ///////////////////////////////////////////////////////////////////////////////////////  
                            //RREP
                        } else if (pl[0] == 3) {
                            for (int i = 0; i < ARRAY_SIZE; i++) {
                                if (routeDiscoveryTable[i].dest == deciphered.src &&
                                    routeDiscoveryTable[i].src == deciphered.dest) {
                                    routeIndex = i;
                                    previous = rx16.getRemoteAddress16();
                                    updateRouteTable();
                                    break;
                                }
                            }
                        }
                    }
                } 
      
                /***********************************************************/
                /* Prüfung der empfangenen Daten beendet */
                /***********************************************************/
                if (flag == 0) {
                    cipherPayload(index);
                    sendTx();
                }
                address = BROADCAST;
                routed = 0;
                routeIndex = 0;
                previous = 0;
                /********************************************************************************/
                              /* Senden der Daten fertig */
                /********************************************************************************/
            }
        } else {
            // not something we were expecting
            Serial.print("NOTHING EXPECTED RECEIVED\r\n");
        }
        Serial.print("\r\n");
    } else if (xbee.getResponse().isError()) {
        Serial.print("ERROR RECEIVED:");
        Serial.print(xbee.getResponse().getErrorCode());
        //nss.print("Error reading packet.  Error code: ");  
        //nss.println(xbee.getResponse().getErrorCode());
        // or flash error led
    }   
    i += 1;
}
