/*
Copyright (C) 2020  "Ju@Workshop" <ju82@free.fr>
License: GPL Version 3
*/
/*
#include <WiFiUdp.h>
#include <WiFiType.h>
#include <WiFiSTA.h>
#include <WiFiServer.h>
#include <WiFiScan.h>
#include <WiFiMulti.h>
#include <WiFiGeneric.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ETH.h>
*/


#include <WiFi.h>

#include <PubSubClient.h>
#include <CAN.h>
#include "Parameters.h"


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



//variables for VE can


//#include <FlexCAN_T4.h>

//FlexCAN_T4<CAN0, RX_SIZE_256, TX_SIZE_16> VeCan;

// Even if no BMS response.. send MQTT and print
#define PRINT_SERIAL_AND_SEND_MQTT_ANYWAY true


// Update with your Wifi details
/*
#define WIFI_SSID		"NoDeal_EXT"
#define WIFI_PASSWORD	"J6786yyy"

// Update with your MQTT Broker details
#define MQTT_SERVER	"192.168.1.253"
#define MQTT_PORT	1883
#define MQTT_USERNAME	"battery"			// Empty string for none.
#define MQTT_PASSWORD	"Switch1"
*/

#define WIFI_SSID		"Stardust"
#define WIFI_PASSWORD	"Sniegulinka1983"

// Update with your MQTT Broker details
#define MQTT_SERVER	"192.168.1.135"
#define MQTT_PORT	1883
#define MQTT_USERNAME	"Alpha"			// Empty string for none.
#define MQTT_PASSWORD	"Inverter1"



// The device name is used as the MQTT base topic and presence on the network.
// If you need more than one AntBmsToCan on your network, give them unique names.
#define DEVICE_NAME "AntBmsToCan"

#define MQTT_BMS_DETAILS "/bms/details"

#define OLED_CHARACTER_WIDTH 11

#define BMS_RX_PIN 16
#define BMS_TX_PIN 17

#define CAN_RX_PIN 26
#define CAN_TX_PIN 27


#define CAN_BAUD_RATE 500000
#define BMS_BAUD_RATE 19200
#define ESP_DEBUGGING_BAUD_RATE 115200

#define KELVIN_OFFSET 273.15



// On boot will request a buffer size of (MAX_MQTT_PAYLOAD_SIZE + MQTT_HEADER_SIZE) for MQTT, and
// MAX_MQTT_PAYLOAD_SIZE for building payloads.  If these fail and your device doesn't boot, you can assume you've set this too high.
#define MAX_MQTT_PAYLOAD_SIZE 4096
#define MIN_MQTT_PAYLOAD_SIZE 512
#define MQTT_HEADER_SIZE 512


//HardwareSerial _can(1); // UART 1 Pins Remapped (GPIO26 RX2, GPIO27 TX2)
HardwareSerial _bms(2); // UART 2 (GPIO16 RX2, GPIO17 TX2)


unsigned long _validResponseCounter = 0;
unsigned long _invalidResponseCounter = 0;


// WiFi parameters
WiFiClient _wifi;


// MQTT parameters
PubSubClient _mqtt(_wifi);

// Buffer Size (and therefore payload size calc)
int _bufferSize;
int _maxPayloadSize;


// I want to declare this once at a modular level, keep the heap somewhere in check.
char* _mqttPayload;



// Code to Lookup for CHARGE MOSFET STATUS
static const uint8_t CHARGE_MOSFET_STATUS_SIZE = 16;
static const char* const CHARGE_MOSFET_STATUS[CHARGE_MOSFET_STATUS_SIZE] = {
    "Off",                           // 0x00
    "On",                            // 0x01
    "Overcharge protection",         // 0x02
    "Over current protection",       // 0x03
    "Battery full",                  // 0x04
    "Total overpressure",            // 0x05
    "Battery over temperature",      // 0x06
    "MOSFET over temperature",       // 0x07
    "Abnormal current",              // 0x08
    "Balanced line dropped string",  // 0x09
    "Motherboard over temperature",  // 0x0A
    "Unknown",                       // 0x0B
    "Unknown",                       // 0x0C
    "Discharge MOSFET abnormality",  // 0x0D
    "Unknown",                       // 0x0E
    "Manually turned off",           // 0x0F
};

// Code to Lookup for DISCHARGE MOSFET STATUS
static const uint8_t DISCHARGE_MOSFET_STATUS_SIZE = 16;
static const char* const DISCHARGE_MOSFET_STATUS[DISCHARGE_MOSFET_STATUS_SIZE] = {
    "Off",                           // 0x00
    "On",                            // 0x01
    "Overdischarge protection",      // 0x02
    "Over current protection",       // 0x03
    "Unknown",                       // 0x04
    "Total pressure undervoltage",   // 0x05
    "Battery over temperature",      // 0x06
    "MOSFET over temperature",       // 0x07
    "Abnormal current",              // 0x08
    "Balanced line dropped string",  // 0x09
    "Motherboard over temperature",  // 0x0A
    "Charge MOSFET on",              // 0x0B
    "Short circuit protection",      // 0x0C
    "Discharge MOSFET abnormality",  // 0x0D
    "Start exception",               // 0x0E
    "Manually turned off",           // 0x0F
};

// Code to Lookup for BALANCER STATUS
static const uint8_t BALANCER_STATUS_SIZE = 11;
static const char* const BALANCER_STATUS[BALANCER_STATUS_SIZE] = {
    "Off",                                   // 0x00
    "Exceeds the limit equilibrium",         // 0x01
    "Charge differential pressure balance",  // 0x02
    "Balanced over temperature",             // 0x03
    "Automatic equalization",                // 0x04
    "Unknown",                               // 0x05
    "Unknown",                               // 0x06
    "Unknown",                               // 0x07
    "Unknown",                               // 0x08
    "Unknown",                               // 0x09
    "Motherboard over temperature",          // 0x0A
};




struct bmsMessage
{
    // Details from the BMS
    int nbr_cell;
    int soc;
    int cell_av;
    int cell_min;
    int cell_max;
    int cell_1;
    int cell_2;
    int cell_3;
    int cell_4;
    int cell_5;
    int cell_6;
    int cell_7;
    int cell_8;
    int cell_9;
    int cell_10;
    int cell_11;
    int cell_12;
    int cell_13;
    int cell_14;
    int cell_15;
    int cell_16;
    int cell_17;
    int cell_18;
    int cell_19;
    int cell_20;
    int cell_21;
    int cell_22;
    int cell_23;
    int cell_24;
    int t_bal;
    int t_fet;
    int t_1;
    int t_2;
    int t_3;
    int t_4;
    int puiss;
    int HighTemp;
    int LowTemp;

    float volt;
    float amp;
    float Ah_install;
    float Ah_rest;
    float Ah_Total;
};

bmsMessage _receivedMessage;

// These timers are used in the main loop.
#define BMS_QUERY_INTERVAL 1000
#define BMS_TIMEOUT 250


#define BMS_MESSAGE_LENGTH 140



// Wemos OLED Shield set up. 64x48
// Pins D1 D2 if ESP8266
// Pins GPIO22 and GPIO21 (SCL/SDA) with optional reset on GPIO13 if ESP32
Adafruit_SSD1306 _display(-1); // No RESET Pin

// OLED variables
char _oledOperatingIndicator = '*';
char _oledLine2[OLED_CHARACTER_WIDTH] = "";
char _oledLine3[OLED_CHARACTER_WIDTH] = "";
char _oledLine4[OLED_CHARACTER_WIDTH] = "";






void sendCanMessage()
{
    uint16_t SOH = 100; // SOH place holder

    unsigned char alarm[4] = { 0, 0, 0, 0 };
    unsigned char warning[4] = { 0, 0, 0, 0 };
    unsigned char mes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    // unsigned char bmsname[8] = { '@', 'A', 'n', 't', 'B', 'm', 's', '@' };
    unsigned char bmsname[8] = { 'P', 'Y', 'L', 'O', 'N', ' ', 0, 0 };

    CAN.beginExtendedPacket(0x351);
    CAN.write(lowByte(uint16_t(CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100));
    CAN.write(highByte(uint16_t(CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100));
    CAN.write(lowByte(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // chargecurrent
    CAN.write(highByte(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // chargecurrent
    CAN.write(lowByte(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // dischargecurrent
    CAN.write(highByte(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // dischargecurrent
    CAN.write(lowByte(uint16_t(DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100));
    CAN.write(highByte(uint16_t(DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100));
    CAN.endPacket();

    /*
    CAN_message_t msg;
    msg.id = 0x351;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100);
    msg.buf[1] = highByte(uint16_t(CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100);
    msg.buf[2] = lowByte(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP); // chargecurrent
    msg.buf[3] = highByte(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP); // chargecurrent
    msg.buf[4] = lowByte(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP); // dischargecurrent
    msg.buf[5] = highByte(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP); // dischargecurrent
    msg.buf[6] = lowByte(uint16_t(DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100);
    msg.buf[7] = highByte(uint16_t(DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedMessage.nbr_cell) / 100);
    VeCan.write(msg);
    */


    delay(2);

    CAN.beginExtendedPacket(0x355);
    CAN.write(lowByte(_receivedMessage.soc));//SOC
    CAN.write(highByte(_receivedMessage.soc));//SOC
    CAN.write(lowByte(SOH));//SOH
    CAN.write(highByte(SOH));//SOH
    CAN.write(0);//SOC
    CAN.write(0);//SOC
    CAN.write(0);
    CAN.write(0);
    CAN.endPacket();

    /*
    msg.id = 0x355;
    msg.len = 8;
    msg.buf[0] = lowByte(_receivedMessage.soc);//SOC
    msg.buf[1] = highByte(_receivedMessage.soc);//SOC
    msg.buf[2] = lowByte(SOH);//SOH
    msg.buf[3] = highByte(SOH);//SOH
    msg.buf[4] = lowByte(_receivedMessage.soc * 10);//SOC
    msg.buf[5] = highByte(_receivedMessage.soc * 10);//SOC
    msg.buf[6] = 0;
    msg.buf[7] = 0;
    VeCan.write(msg);
    */

 
    delay(2);

    CAN.beginExtendedPacket(0x356);
    CAN.write(lowByte(uint16_t(_receivedMessage.volt * 10)));
    CAN.write(highByte(uint16_t(_receivedMessage.volt * 10)));
    CAN.write(lowByte(long(-_receivedMessage.amp)));
    CAN.write(highByte(long(-_receivedMessage.amp)));
    CAN.write(lowByte(int16_t(_receivedMessage.t_fet * 10)));
    CAN.write(highByte(int16_t(_receivedMessage.t_fet * 10)));
    CAN.write(0);
    CAN.write(0);
    CAN.endPacket();

    /*
    msg.id = 0x356;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(_receivedMessage.volt * 10));
    msg.buf[1] = highByte(uint16_t(_receivedMessage.volt * 10));
    msg.buf[2] = lowByte(long(-_receivedMessage.amp));
    msg.buf[3] = highByte(long(-_receivedMessage.amp));
    msg.buf[4] = lowByte(int16_t(_receivedMessage.t_fet * 10));
    msg.buf[5] = highByte(int16_t(_receivedMessage.t_fet * 10));
    msg.buf[6] = 0;
    msg.buf[7] = 0;
    VeCan.write(msg);
    */

    // Spec for Pylontech addresses this as 0x359
    delay(2);

    CAN.beginExtendedPacket(0x359);
    CAN.write(alarm[0]);
    CAN.write(alarm[1]);
    CAN.write(alarm[2]);
    CAN.write(alarm[3]);
    CAN.write(warning[1]);
    CAN.write(warning[2]);
    CAN.write(warning[3]);
    CAN.write(warning[4]);
    CAN.endPacket();

    /*
    msg.id = 0x35A;
    msg.len = 8;
    msg.buf[0] = alarm[0];
    msg.buf[1] = alarm[1];
    msg.buf[2] = alarm[2];
    msg.buf[3] = alarm[3];
    msg.buf[4] = warning[0];
    msg.buf[5] = warning[1];
    msg.buf[6] = warning[2];
    msg.buf[7] = warning[3];
    VeCan.write(msg);
    */


    delay(2);

    CAN.beginExtendedPacket(0x35E);
    CAN.write(bmsname[0]);
    CAN.write(bmsname[1]);
    CAN.write(bmsname[2]);
    CAN.write(bmsname[3]);
    CAN.write(bmsname[4]);
    CAN.write(bmsname[5]);
    CAN.write(bmsname[6]);
    CAN.write(bmsname[7]);
    CAN.endPacket();

    /*
    msg.id = 0x35E;
    msg.len = 8;
    msg.buf[0] = bmsname[0];
    msg.buf[1] = bmsname[1];
    msg.buf[2] = bmsname[2];
    msg.buf[3] = bmsname[3];
    msg.buf[4] = bmsname[4];
    msg.buf[5] = bmsname[5];
    msg.buf[6] = bmsname[6];
    msg.buf[7] = bmsname[7];
    VeCan.write(msg);
    */


    //  delay(2);
    /*
    CAN.beginExtendedPacket(0x370);
    CAN.write(bmsmanu[0]);
    CAN.write(bmsmanu[1]);
    CAN.write(bmsmanu[2]);
    CAN.write(bmsmanu[3]);
    CAN.write(bmsmanu[4]);
    CAN.write(bmsmanu[5]);
    CAN.write(bmsmanu[6]);
    CAN.write(bmsmanu[7]);
    CAN.endPacket();
    */

    /*
    msg.id  = 0x370;
    msg.len = 8;
    msg.buf[0] = bmsmanu[0];
    msg.buf[1] = bmsmanu[1];
    msg.buf[2] = bmsmanu[2];
    msg.buf[3] = bmsmanu[3];
    msg.buf[4] = bmsmanu[4];
    msg.buf[5] = bmsmanu[5];
    msg.buf[6] = bmsmanu[6];
    msg.buf[7] = bmsmanu[7];
    VeCan.write(msg);
    */

    // Not in Pylontech Spec
    /*
    delay(2);

    CAN.beginExtendedPacket(0x373);
    CAN.write(lowByte(uint16_t(_receivedMessage.cell_min)));
    CAN.write(highByte(uint16_t(_receivedMessage.cell_min)));
    CAN.write(lowByte(uint16_t(_receivedMessage.cell_max)));
    CAN.write(highByte(uint16_t(_receivedMessage.cell_max)));
    CAN.write(lowByte(uint16_t(_receivedMessage.LowTemp + KELVIN_OFFSET)));
    CAN.write(highByte(uint16_t(_receivedMessage.LowTemp + KELVIN_OFFSET)));
    CAN.write(lowByte(uint16_t(_receivedMessage.HighTemp + KELVIN_OFFSET)));
    CAN.write(highByte(uint16_t(_receivedMessage.HighTemp + KELVIN_OFFSET)));
    CAN.endPacket();
    */


    /*
    msg.id = 0x373;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(_receivedMessage.cell_min));
    msg.buf[1] = highByte(uint16_t(_receivedMessage.cell_min));
    msg.buf[2] = lowByte(uint16_t(_receivedMessage.cell_max));
    msg.buf[3] = highByte(uint16_t(_receivedMessage.cell_max));
    msg.buf[4] = lowByte(uint16_t(_receivedMessage.LowTemp + 273.15));
    msg.buf[5] = highByte(uint16_t(_receivedMessage.LowTemp + 273.15));
    msg.buf[6] = lowByte(uint16_t(_receivedMessage.HighTemp + 273.15));
    msg.buf[7] = highByte(uint16_t(_receivedMessage.HighTemp + 273.15));
    VeCan.write(msg);
    */

    // Not in Pylontech Spec
    /*
    delay(2);

    CAN.beginExtendedPacket(0x372);
    CAN.write(lowByte(_receivedMessage.nbr_cell)); //bms.getNumModules()
    CAN.write(highByte(_receivedMessage.nbr_cell)); //bms.getNumModules()
    CAN.write(0x00);
    CAN.write(0x00);
    CAN.write(0x00);
    CAN.write(0x00);
    CAN.write(0x00);
    CAN.write(0x00);
    CAN.endPacket();
    */



    /*
    msg.id = 0x372;
    msg.len = 8;
    msg.buf[0] = lowByte(_receivedMessage.nbr_cell);//bms.getNumModules()
    msg.buf[1] = highByte(_receivedMessage.nbr_cell);//bms.getNumModules()
    msg.buf[2] = 0x00;
    msg.buf[3] = 0x00;
    msg.buf[4] = 0x00;
    msg.buf[5] = 0x00;
    msg.buf[6] = 0x00;
    msg.buf[7] = 0x00;
    VeCan.write(msg);
    */
}




void printValuesToSerialAndSendMQTT()
{
    long milli_start = millis();
    char topicResponse[100] = ""; // 100 should cover a topic name
    char stateAddition[256] = ""; // 256 should cover individual additions to be added to the payload.

    strcpy(topicResponse, DEVICE_NAME MQTT_BMS_DETAILS);

    Serial.print((_receivedMessage.nbr_cell), DEC); Serial.println(" cells");
    Serial.print("soc = ");Serial.print((_receivedMessage.soc), DEC);Serial.println("%");
    Serial.print("Voltage = ");Serial.print((_receivedMessage.volt / 10), DEC);Serial.println(" V");
    Serial.print("Amperes = ");Serial.print((_receivedMessage.amp / 10), DEC);Serial.println(" A");
    Serial.print("Puissance = ");Serial.print((_receivedMessage.puiss), DEC);Serial.println(" Wh");
    Serial.print("Cell_av = ");Serial.print((_receivedMessage.cell_av), DEC);Serial.println(" mV");
    Serial.print("Cell_min = ");Serial.print((_receivedMessage.cell_min), DEC);Serial.println(" mV");
    Serial.print("Cell_max = ");Serial.print((_receivedMessage.cell_max), DEC);Serial.println(" mV");
    Serial.print("T_MOSFET = ");Serial.print((_receivedMessage.t_fet), DEC);Serial.println(" °C");
    Serial.print("T_BALANCE = ");Serial.print((_receivedMessage.t_bal), DEC);Serial.println(" °C");
    Serial.print("T_1 = ");Serial.print((_receivedMessage.t_1), DEC);Serial.println(" °C");
    Serial.print("T_2 = ");Serial.print((_receivedMessage.t_2), DEC);Serial.println(" °C");
    Serial.print("T_3 = ");Serial.print((_receivedMessage.t_3), DEC);Serial.println(" °C");
    Serial.print("T_4 = ");Serial.print((_receivedMessage.t_4), DEC);Serial.println(" °C");
    Serial.print("Cell_1 = ");Serial.print((_receivedMessage.cell_1), DEC);Serial.println(" mV");
    Serial.print("Cell_2 = ");Serial.print((_receivedMessage.cell_2), DEC);Serial.println(" mV");
    Serial.print("Cell_3 = ");Serial.print((_receivedMessage.cell_3), DEC);Serial.println(" mV");
    Serial.print("Cell_4 = ");Serial.print((_receivedMessage.cell_4), DEC);Serial.println(" mV");
    Serial.print("Cell_5 = ");Serial.print((_receivedMessage.cell_5), DEC);Serial.println(" mV");
    Serial.print("Cell_6 = ");Serial.print((_receivedMessage.cell_6), DEC);Serial.println(" mV");
    Serial.print("Cell_7 = ");Serial.print((_receivedMessage.cell_7), DEC);Serial.println(" mV");
    Serial.print("Cell_8 = ");Serial.print((_receivedMessage.cell_8), DEC);Serial.println(" mV");
    Serial.print("Cell_9 = ");Serial.print((_receivedMessage.cell_9), DEC);Serial.println(" mV");
    Serial.print("Cell_10 = ");Serial.print((_receivedMessage.cell_10), DEC);Serial.println(" mV");
    Serial.print("Cell_11 = ");Serial.print((_receivedMessage.cell_11), DEC);Serial.println(" mV");
    Serial.print("Cell_12 = ");Serial.print((_receivedMessage.cell_12), DEC);Serial.println(" mV");
    Serial.print("Cell_13 = ");Serial.print((_receivedMessage.cell_13), DEC);Serial.println(" mV");
    Serial.print("Cell_14 = ");Serial.print((_receivedMessage.cell_14), DEC);Serial.println(" mV");
    Serial.print("Cell_15 = ");Serial.print((_receivedMessage.cell_15), DEC);Serial.println(" mV");
    Serial.print("Cell_16 = ");Serial.print((_receivedMessage.cell_16), DEC);Serial.println(" mV");
    Serial.print("Cell_17 = ");Serial.print((_receivedMessage.cell_17), DEC);Serial.println(" mV");
    Serial.print("Cell_18 = ");Serial.print((_receivedMessage.cell_18), DEC);Serial.println(" mV");
    Serial.print("Cell_19 = ");Serial.print((_receivedMessage.cell_19), DEC);Serial.println(" mV");
    Serial.print("Cell_20 = ");Serial.print((_receivedMessage.cell_20), DEC);Serial.println(" mV");
    Serial.print("Cell_21 = ");Serial.print((_receivedMessage.cell_21), DEC);Serial.println(" mV");
    Serial.print("Cell_22 = ");Serial.print((_receivedMessage.cell_22), DEC);Serial.println(" mV");
    Serial.print("Cell_23 = ");Serial.print((_receivedMessage.cell_23), DEC);Serial.println(" mV");
    Serial.print("Cell_24 = ");Serial.print((_receivedMessage.cell_24), DEC);Serial.println(" mV");
    Serial.print("Ah_install = ");Serial.print((_receivedMessage.Ah_install / 1000000), DEC);Serial.println(" Ah");
    Serial.print("Ah_rest = ");Serial.print((_receivedMessage.Ah_rest / 1000000), DEC);Serial.println(" Ah");
    Serial.print("Ah_Total = ");Serial.print((_receivedMessage.Ah_Total / 1000), DEC);Serial.println(" Ah");
    Serial.print("Valid Responses = ");Serial.print(_validResponseCounter);
    Serial.print("Invalid Responses = ");Serial.print(_invalidResponseCounter);
    Serial.print("Ms = ");Serial.println((millis()) - (milli_start));
    Serial.println("_____________________________________________");



    addToPayload("{");

    sprintf(stateAddition, "    \"nbr_cell\": %d,", _receivedMessage.nbr_cell);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"soc\": %d,", _receivedMessage.soc);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"volt\": %d,", (_receivedMessage.volt / 10));
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"amp\": %d,", (_receivedMessage.amp / 10));
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"puiss\": %d,", _receivedMessage.puiss);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_av\": %d,", _receivedMessage.cell_av);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_min\": %d,", _receivedMessage.cell_min);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_max\": %d,", _receivedMessage.cell_max);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_fet\": %d,", _receivedMessage.t_fet);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_bal\": %d,", _receivedMessage.t_bal);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_1\": %d,", _receivedMessage.t_1);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_2\": %d,", _receivedMessage.t_2);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_3\": %d,", _receivedMessage.t_3);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"t_4\": %d,", _receivedMessage.t_4);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_1\": %d,", _receivedMessage.cell_1);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_2\": %d,", _receivedMessage.cell_2);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_3\": %d,", _receivedMessage.cell_3);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_4\": %d,", _receivedMessage.cell_4);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_5\": %d,", _receivedMessage.cell_5);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_6\": %d,", _receivedMessage.cell_6);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_7\": %d,", _receivedMessage.cell_7);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_8\": %d,", _receivedMessage.cell_8);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_9\": %d,", _receivedMessage.cell_9);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_10\": %d,", _receivedMessage.cell_10);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_11\": %d,", _receivedMessage.cell_11);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_12\": %d,", _receivedMessage.cell_12);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_13\": %d,", _receivedMessage.cell_13);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_14\": %d,", _receivedMessage.cell_14);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_15\": %d,", _receivedMessage.cell_15);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_16\": %d,", _receivedMessage.cell_16);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_17\": %d,", _receivedMessage.cell_17);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_18\": %d,", _receivedMessage.cell_18);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_19\": %d,", _receivedMessage.cell_19);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_20\": %d,", _receivedMessage.cell_20);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_21\": %d,", _receivedMessage.cell_21);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_22\": %d,", _receivedMessage.cell_22);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_23\": %d,", _receivedMessage.cell_23);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"cell_24\": %d,", _receivedMessage.cell_24);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"Ah_install\": %d,", (_receivedMessage.Ah_install / 1000000));
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"Ah_rest\": %d,", (_receivedMessage.Ah_rest / 1000000));
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"Ah_Total\": %d,", (_receivedMessage.Ah_Total / 1000));
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"valid_responses\": %d,", _validResponseCounter);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"invalid_responses\": %d,", _invalidResponseCounter);
    addToPayload(stateAddition);
    sprintf(stateAddition, "    \"ms\": %d", (millis()) - (milli_start));
    addToPayload(stateAddition);

    addToPayload("}");
    sendMqtt(topicResponse);
}




/*
setup

The setup function runs once when you press reset or power the board
*/
void setup()
{
    // UART 0 for USB debugging (GPIO1 TX, GPIO3 RX)
    Serial.begin(ESP_DEBUGGING_BAUD_RATE);

    // UART 2
    _bms.begin(BMS_BAUD_RATE, SERIAL_8N1, BMS_RX_PIN, BMS_TX_PIN); // (19200,SERIAL_8N1,RXD2,TXD2)
    _bms.setTimeout(BMS_TIMEOUT); // How long to wait for received data
   // Serial.println("3");

    // UART 1 remapped to GPIOs
    //_can.begin(CAN_BAUD_RATE, SERIAL_8N1, CAN_RX_PIN, CAN_TX_PIN); // (500000, SERIAL_8N1, GPIO26 as RX1, GPIO27 as TX1)


    // Display time
    _display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize OLED with the I2C addr 0x3C (for the 64x48)
    _display.clearDisplay();
    _display.display();

    updateOLED("", "", "", "");


    // LED Output
    pinMode(LED_BUILTIN, OUTPUT);

    // Configure WIFI
    setupWifi();



    // Configure MQTT to the address and port specified above
    _mqtt.setServer(MQTT_SERVER, MQTT_PORT);
#ifdef DEBUG
    sprintf(_debugOutput, "About to request buffer");
    Serial.println(_debugOutput);
#endif
    for (_bufferSize = (MAX_MQTT_PAYLOAD_SIZE + MQTT_HEADER_SIZE); _bufferSize >= MIN_MQTT_PAYLOAD_SIZE + MQTT_HEADER_SIZE; _bufferSize = _bufferSize - 1024)
    {
#ifdef DEBUG
        sprintf(_debugOutput, "Requesting a buffer of : %d bytes", _bufferSize);
        Serial.println(_debugOutput);
#endif

        if (_mqtt.setBufferSize(_bufferSize))
        {

            _maxPayloadSize = _bufferSize - MQTT_HEADER_SIZE;
#ifdef DEBUG
            sprintf(_debugOutput, "_bufferSize: %d,\r\n\r\n_maxPayload (Including null terminator): %d", _bufferSize, _maxPayloadSize);
            Serial.println(_debugOutput);
#endif

            // Example, 2048, if declared as 2048 is positions 0 to 2047, and position 2047 needs to be zero.  2047 usable chars in payload.
            _mqttPayload = new char[_maxPayloadSize];
            emptyPayload();

            break;
        }
        else
        {
#ifdef DEBUG
            sprintf(_debugOutput, "Coudln't allocate buffer of %d bytes", _bufferSize);
            Serial.println(_debugOutput);
#endif
        }
    }


    // And any messages we are subscribed to will be pushed to the mqttCallback function for processing
    _mqtt.setCallback(mqttCallback);

    // Connect to MQTT
    mqttReconnect();


    // CAN
    updateOLED("Starting", "CAN...", "", "");
    CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);
    while (!CAN.begin(CAN_BAUD_RATE))
    {

    }
    updateOLED("CAN", "Started", "", "");
    delay(500);
}

void loop()
{
    static unsigned long lastBmsQuery = 0;


    // Make sure WiFi is good
    if (WiFi.status() != WL_CONNECTED)
    {
        setupWifi();
    }


    // make sure mqtt is still connected
    if ((!_mqtt.connected()) || !_mqtt.loop())
    {
        mqttReconnect();
    }

    // Time to query BMS?
    if (checkTimer(&lastBmsQuery, BMS_QUERY_INTERVAL))
    {
        readBms();
    }


}







/*
updateOLED

Update the OLED. Use "NULL" for no change to a line or "" for an empty line.
Three parameters representing each of the three lines available for status indication - Top line functionality fixed
*/
void updateOLED(const char* line1, const char* line2, const char* line3, const char* line4)
{
    static unsigned long updateStatusBar = 0;


    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(WHITE);
    _display.setCursor(0, 0);

    char line1Contents[OLED_CHARACTER_WIDTH];
    char line2Contents[OLED_CHARACTER_WIDTH];
    char line3Contents[OLED_CHARACTER_WIDTH];
    char line4Contents[OLED_CHARACTER_WIDTH];

    // Ensure only dealing with 10 chars passed in, and null terminate.
    if (strlen(line1) > OLED_CHARACTER_WIDTH - 1)
    {
        strncpy(line1Contents, line1, OLED_CHARACTER_WIDTH - 1);
        line1Contents[OLED_CHARACTER_WIDTH - 1] = 0;
    }
    else
    {
        strcpy(line1Contents, line1);
    }


    if (strlen(line2) > OLED_CHARACTER_WIDTH - 1)
    {
        strncpy(line2Contents, line2, OLED_CHARACTER_WIDTH - 1);
        line2Contents[OLED_CHARACTER_WIDTH - 1] = 0;
    }
    else
    {
        strcpy(line2Contents, line2);
    }


    if (strlen(line3) > OLED_CHARACTER_WIDTH - 1)
    {
        strncpy(line3Contents, line3, OLED_CHARACTER_WIDTH - 1);
        line3Contents[OLED_CHARACTER_WIDTH - 1] = 0;
    }
    else
    {
        strcpy(line3Contents, line3);
    }


    if (strlen(line4) > OLED_CHARACTER_WIDTH - 1)
    {
        strncpy(line4Contents, line4, OLED_CHARACTER_WIDTH - 1);
        line4Contents[OLED_CHARACTER_WIDTH - 1] = 0;
    }
    else
    {
        strcpy(line4Contents, line4);
    }


    _display.println(line1Contents);

    // Next line
    _display.setCursor(0, 12);
    _display.println(line2Contents);

    _display.setCursor(0, 24);
    _display.println(line3Contents);

    _display.setCursor(0, 36);
    _display.println(line4Contents);

    // Refresh the display
    _display.display();
}



/*
checkTimer

Check to see if the elapsed interval has passed since the passed in millis() value. If it has, return true and update the lastRun.
Note that millis() overflows after 50 days, so we need to deal with that too... in our case we just zero the last run, which means the timer
could be shorter but it's not critical... not worth the extra effort of doing it properly for once in 50 days.
*/
bool checkTimer(unsigned long* lastRun, unsigned long interval)
{
    unsigned long now = millis();

    if (*lastRun > now)
        *lastRun = 0;

    if (now >= *lastRun + interval)
    {
        *lastRun = now;
        return true;
    }

    return false;
}




//  Byte    Address     Size        Description         Decoded content                         Coeff./Unit
//  0       0x00        1           Header bytes        AA 55 AA FF
#define ANT_BYTENUM_HEADER_1 0
#define ANT_BYTENUM_HEADER_2 1
#define ANT_BYTENUM_HEADER_3 2
#define ANT_BYTENUM_HEADER_4 3

//  4       0x02        2           Total voltage       541 * 0.1 = 54.1V                       0.1 V
#define ANT_BYTENUM_TOTAL_VOLTAGE 4

//  6       0x06        2           Cell voltage 1      4138 * 0.001 = 4.138V                   0.001 V
#define ANT_BYTENUM_CELL_VOLTAGE_1 6
#define ANT_BYTENUM_CELL_VOLTAGE_2 8
#define ANT_BYTENUM_CELL_VOLTAGE_3 10
#define ANT_BYTENUM_CELL_VOLTAGE_4 12
#define ANT_BYTENUM_CELL_VOLTAGE_5 14
#define ANT_BYTENUM_CELL_VOLTAGE_6 16
#define ANT_BYTENUM_CELL_VOLTAGE_7 18
#define ANT_BYTENUM_CELL_VOLTAGE_8 20
#define ANT_BYTENUM_CELL_VOLTAGE_9 22
#define ANT_BYTENUM_CELL_VOLTAGE_10 24
#define ANT_BYTENUM_CELL_VOLTAGE_11 26
#define ANT_BYTENUM_CELL_VOLTAGE_12 28
#define ANT_BYTENUM_CELL_VOLTAGE_13 30
#define ANT_BYTENUM_CELL_VOLTAGE_14 32
#define ANT_BYTENUM_CELL_VOLTAGE_15 34
#define ANT_BYTENUM_CELL_VOLTAGE_16 36
#define ANT_BYTENUM_CELL_VOLTAGE_17 38
#define ANT_BYTENUM_CELL_VOLTAGE_18 40
#define ANT_BYTENUM_CELL_VOLTAGE_19 42
#define ANT_BYTENUM_CELL_VOLTAGE_20 44
#define ANT_BYTENUM_CELL_VOLTAGE_21 46
#define ANT_BYTENUM_CELL_VOLTAGE_22 48
#define ANT_BYTENUM_CELL_VOLTAGE_23 50
#define ANT_BYTENUM_CELL_VOLTAGE_24 52
#define ANT_BYTENUM_CELL_VOLTAGE_25 54
#define ANT_BYTENUM_CELL_VOLTAGE_26 56
#define ANT_BYTENUM_CELL_VOLTAGE_27 58
#define ANT_BYTENUM_CELL_VOLTAGE_28 60
#define ANT_BYTENUM_CELL_VOLTAGE_29 62
#define ANT_BYTENUM_CELL_VOLTAGE_30 64
#define ANT_BYTENUM_CELL_VOLTAGE_31 66
#define ANT_BYTENUM_CELL_VOLTAGE_32 68

//  70      0x46        4           Current             0.0A                                    0.1 A
#define ANT_BYTENUM_CURRENT 70

//  74      0x4A        1           SOC                 100%                                    1.0 %
#define ANT_BYTENUM_SOC 74

//  75      0x4B        4           Total Battery Capacity Setting
#define ANT_BYTENUM_TOTAL_BATTERY_CAPACITY_SETTING      39000000 x



void readBms()
{
    // AntBms Request Data Command
    const byte requestCommand[] = { 0x5A, 0x5A, 0x00, 0x00, 0x01, 0x01 };

    // Received Messages begin with this
    const byte startMark[] = { 0xAA, 0x55, 0xAA, 0xFF };

    // Buffer for incoming data
    byte incomingBuffer[BMS_MESSAGE_LENGTH] = { 0 };

    // For printing the input to the serial monitor
    char hexChar[3];

    // For temporary calculations
    uint32_t tempCalc = 0;


    auto ant_get_16bit = [&](size_t i) -> uint16_t {
        return (uint16_t(incomingBuffer[i + 0]) << 8) | (uint16_t(incomingBuffer[i + 1]) << 0);
    };
    auto ant_get_32bit = [&](size_t i) -> uint32_t {
        return (uint32_t(ant_get_16bit(i + 0)) << 16) | (uint32_t(ant_get_16bit(i + 2)) << 0);
    };

    //updateOLED("Flush", "Serial", "", "");
    // 
    // 
    // Clear out outbound and inbound buffers
    _bms.flush();
    while (_bms.available())
    {
        _bms.read();
    }

    //updateOLED("BMS", "Request", "", "");
    // Send the request command
    _bms.write(requestCommand, sizeof(requestCommand)); // while (Serial2.available()) {  Serial.print(Serial2.read()); };
    _bms.flush(); // Ensure sent on its way


    //updateOLED("Wait", "BMS", "Response", "");h 
    _bms.readBytes(incomingBuffer, BMS_MESSAGE_LENGTH); // Will read up to 140 chars or timeout

    Serial.println("--- Raw Bytes ---");
    for (uint8_t i = 0; i < sizeof(incomingBuffer); i++)
    {
        sprintf(hexChar, "%02X", incomingBuffer[i]);
        Serial.print(hexChar);
        if (i < sizeof(incomingBuffer) - 1)
        {
            Serial.print(' ');
        }
    }
    Serial.println("--- End Raw Bytes ---");


    // First check CRC
    if (checkCRC(incomingBuffer, BMS_MESSAGE_LENGTH))
    {

        // So here we can run the test
        if (incomingBuffer[ANT_BYTENUM_HEADER_1] == startMark[ANT_BYTENUM_HEADER_1] && incomingBuffer[ANT_BYTENUM_HEADER_2] == startMark[ANT_BYTENUM_HEADER_2] && incomingBuffer[ANT_BYTENUM_HEADER_3] == startMark[ANT_BYTENUM_HEADER_3] && incomingBuffer[ANT_BYTENUM_HEADER_4] == startMark[ANT_BYTENUM_HEADER_4])
        {
            // Valid start, lets get the rest
            _validResponseCounter++;


            // Status response
            // <- 0xAA 0x55 0xAA 0xFF: Header
            //      0    1    2    3
            // *Data*
            //
            // Byte   Address Content: Description      Decoded content                         Coeff./Unit

            //   4    0x02 0x1D: Total voltage         541 * 0.1 = 54.1V                        0.1 V
            float total_voltage = ant_get_16bit(4) * 0.1f;
            bmsResponse.total_voltage = total_voltage; // float
            //   6    0x10 0x2A: Cell voltage 1        4138 * 0.001 = 4.138V                    0.001 V
            //   8    0x10 0x2A: Cell voltage 2                4138 * 0.001 = 4.138V            0.001 V
            //  10    0x10 0x27: Cell voltage 3                4135 * 0.001 = 4.135V            0.001 V
            //  12    0x10 0x2A: Cell voltage 4                                                 0.001 V
            //  ...
            //  ...
            //  66    0x00 0x00: Cell voltage 31                                                0.001 V
            //  68    0x00 0x00: Cell voltage 32                                                0.001 V
            uint8_t cells = incomingBuffer[123];
            bmsResponse.cells = cells; // uint8_t

            for (uint8_t i = 0; i < cells; i++)
            {
                bmsResponse.cell_voltage[i] = (float)ant_get_16bit(i * 2 + 6) * 0.001f; // float
            }

            //  70    0x00 0x00 0x00 0x00: Current               0.0 A                          0.1 A
            float current = ((int32_t)ant_get_32bit(70)) * 0.1f;
            bmsResponse.current = current; // float
            //  74    0x64: SOC                                  100 %                          1.0 %
            bmsResponse.soc = (float)incomingBuffer[74]; // float
            //  75    0x02 0x53 0x17 0xC0: Total Battery Capacity Setting   39000000            0.000001 Ah
            bmsResponse.total_battery_capacity_setting = (float)ant_get_32bit(75) * 0.000001f; // float
            //  79    0x02 0x53 0x06 0x11: Battery Capacity Remaining                           0.000001 Ah
            bmsResponse.capacity_remaining = (float)ant_get_32bit(79) * 0.000001f; // float
            //  83    0x00 0x08 0xC7 0x8E: Battery Cycle Capacity                               0.001 Ah
            bmsResponse.battery_cycle_capacity = (float)ant_get_32bit(83) * 0.001f; // float
            //  87    0x00 0x08 0x57 0x20: Uptime in seconds     546.592s                       1.0 s
            bmsResponse.total_runtime = (float)ant_get_32bit(87); // float

            /*
            if (this->total_runtime_formatted_text_sensor_ != nullptr)
            {
                this->publish_state_(this->total_runtime_formatted_text_sensor_, format_total_runtime_(ant_get_32bit(87)));
            }
            */

            //  91    0x00 0x15: Temperature 1                   21°C                           1.0 °C
            //  93    0x00 0x15: Temperature 2                   21°C                           1.0 °C
            //  95    0x00 0x14: Temperature 3                   20°C                           1.0 °C
            //  97    0x00 0x14: Temperature 4                   20°C                           1.0 °C
            //  99    0x00 0x14: Temperature 5                   20°C                           1.0 °C
            //  101   0x00 0x14: Temperature 6                   20°C                           1.0 °C
            for (uint8_t i = 0; i < 6; i++)
            {
                bmsResponse.temperatures[i] = (float)((int16_t)ant_get_16bit(i * 2 + 91)); // float
            }

            //  103   0x01: Charge MOSFET Status
            uint8_t raw_charge_mosfet_status = incomingBuffer[103];
            bmsResponse.charge_mosfet_status = (float)raw_charge_mosfet_status; // float

            if (raw_charge_mosfet_status < CHARGE_MOSFET_STATUS_SIZE)
            {
                bmsResponse.charge_mosfet_status_text = CHARGE_MOSFET_STATUS[raw_charge_mosfet_status]; // Char Pointer
            }
            else
            {
                bmsResponse.charge_mosfet_status_text = "Unknown"; // Char Pointer
            }
            bmsResponse.charging_switch = (bool)(raw_charge_mosfet_status == 0x01); // bool


            //  104   0x01: Discharge MOSFET Status
            uint8_t raw_discharge_mosfet_status = incomingBuffer[104];
            bmsResponse.charge_mosfet_status = (float)raw_discharge_mosfet_status; // float

            if (raw_discharge_mosfet_status < DISCHARGE_MOSFET_STATUS_SIZE)
            {
                bmsResponse.discharge_mosfet_status_text = DISCHARGE_MOSFET_STATUS[raw_discharge_mosfet_status]; // Char Pointer
            }
            else
            {
                bmsResponse.discharge_mosfet_status_text = "Unknown"; // Char Pointer
            }
            bmsResponse.discharging_switch = (bool)(raw_charge_mosfet_status == 0x01); // bool

            //  105   0x00: Balancer Status
            uint8_t raw_balancer_status = incomingBuffer[105];
            bmsResponse.balancer_status_code = (float)raw_balancer_status; // float
            bmsResponse.balancer_switch = (bool)(raw_balancer_status == 0x04); // bool

            if (raw_balancer_status < BALANCER_STATUS_SIZE)
            {
                bmsResponse.balancer_status_tex = BALANCER_STATUS[raw_balancer_status]; // Char Pointer
            }
            else
            {
                bmsResponse.balancer_status_tex = "Unknown"; // Char Pointer
            }

            //  106   0x03 0xE8: Tire length                                                    mm
            //  108   0x00 0x17: Number of pulses per week
            //  110   0x01: Relay switch
            //  111   0x00 0x00 0x00 0x00: Current power         0W                             1.0 W
            if (this->supports_new_commands_) {
                this->publish_state_(this->power_sensor_, total_voltage * current);
            }
            else {
                this->publish_state_(this->power_sensor_, (float)(int32_t)ant_get_32bit(111));
            }
            //  115   0x0D: Cell with the highest voltage        Cell 13
            this->publish_state_(this->max_voltage_cell_sensor_, (float)data[115]);
            //  116   0x10 0x2C: Maximum cell voltage            4140 * 0.001 = 4.140V          0.001 V
            float max_cell_voltage = ant_get_16bit(116) * 0.001f;
            this->publish_state_(this->max_cell_voltage_sensor_, max_cell_voltage);
            //  118   0x09: Cell with the lowest voltage         Cell 9
            this->publish_state_(this->min_voltage_cell_sensor_, (float)data[118]);
            //  119   0x10 0x26: Minimum cell voltage            4134 * 0.001 = 4.134V          0.001 V
            float min_cell_voltage = ant_get_16bit(119) * 0.001f;
            this->publish_state_(this->min_cell_voltage_sensor_, min_cell_voltage);
            this->publish_state_(this->delta_cell_voltage_sensor_, max_cell_voltage - min_cell_voltage);
            //  121   0x10 0x28: Average cell voltage            4136 * 0.001 = 4.136V          0.001 V
            this->publish_state_(this->average_cell_voltage_sensor_, ant_get_16bit(121) * 0.001f);
            //  123   0x0D: Battery strings                      13
            this->publish_state_(this->battery_strings_sensor_, (float)data[123]);
            //  124   0x00 0x00: Discharge MOSFET, voltage between D-S                          0.1 V
            //  126   0x00 0x73: Drive voltage (discharge MOSFET)                               0.1 V
            //  128   0x00 0x6F: Drive voltage (charge MOSFET)                                  0.1 V
            //  130   0x02 0xA7: When the detected current is 0, the initial value of the comparator
            //  132   0x00 0x00 0x00 0x00: Battery is in balance bitmask (Bit 1 = Cell 1, Bit 2 = Cell 2, ...)
            //  136   0x11 0x62: System log / overall status bitmask?
            //  138   0x0B 0x00: CRC



            /*
            tempCalc = ((((uint8_t)incomingBuffer[70]) << 24) + (((uint8_t)incomingBuffer[71]) << 16) + (((uint8_t)incomingBuffer[72]) << 8) + ((uint8_t)incomingBuffer[73]));

            if (tempCalc > 2147483648)
            {
                _receivedMessage.amp = (-(2 * 2147483648) + tempCalc);
            }
            else
            {
                _receivedMessage.amp = tempCalc;
            }

            tempCalc = ((((uint8_t)incomingBuffer[111]) << 24) + (((uint8_t)incomingBuffer[112]) << 16) + (((uint8_t)incomingBuffer[113]) << 8) + ((uint8_t)incomingBuffer[114]));
            if (tempCalc > 2147483648)
            {
                _receivedMessage.puiss = (-(2 * 2147483648) + tempCalc);
            }
            else
            {
                _receivedMessage.puiss = tempCalc;
            }


            _receivedMessage.nbr_cell = (uint8_t)incomingBuffer[123];
            _receivedMessage.soc = (uint8_t)incomingBuffer[74];
            _receivedMessage.volt = ((((uint8_t)incomingBuffer[4]) << 8) + (uint8_t)incomingBuffer[5]);
            _receivedMessage.cell_av = ((((uint8_t)incomingBuffer[121]) << 8) + (uint8_t)incomingBuffer[122]);
            _receivedMessage.cell_min = ((((uint8_t)incomingBuffer[119]) << 8) + (uint8_t)incomingBuffer[120]);
            _receivedMessage.cell_max = ((((uint8_t)incomingBuffer[116]) << 8) + (uint8_t)incomingBuffer[117]);
            _receivedMessage.t_fet = ((((uint8_t)incomingBuffer[91]) << 8) + (uint8_t)incomingBuffer[92]);
            _receivedMessage.t_bal = ((((uint8_t)incomingBuffer[93]) << 8) + (uint8_t)incomingBuffer[94]);
            _receivedMessage.t_1 = ((((uint8_t)incomingBuffer[95]) << 8) + (uint8_t)incomingBuffer[96]);
            _receivedMessage.t_2 = ((((uint8_t)incomingBuffer[97]) << 8) + (uint8_t)incomingBuffer[98]);
            _receivedMessage.t_3 = ((((uint8_t)incomingBuffer[99]) << 8) + (uint8_t)incomingBuffer[100]);
            _receivedMessage.t_4 = ((((uint8_t)incomingBuffer[101]) << 8) + (uint8_t)incomingBuffer[102]);
            _receivedMessage.cell_1 = ((((uint8_t)incomingBuffer[6]) << 8) + (uint8_t)incomingBuffer[7]);
            _receivedMessage.cell_2 = ((((uint8_t)incomingBuffer[8]) << 8) + (uint8_t)incomingBuffer[9]);
            _receivedMessage.cell_3 = ((((uint8_t)incomingBuffer[10]) << 8) + (uint8_t)incomingBuffer[11]);
            _receivedMessage.cell_4 = ((((uint8_t)incomingBuffer[12]) << 8) + (uint8_t)incomingBuffer[13]);
            _receivedMessage.cell_5 = ((((uint8_t)incomingBuffer[14]) << 8) + (uint8_t)incomingBuffer[15]);
            _receivedMessage.cell_6 = ((((uint8_t)incomingBuffer[16]) << 8) + (uint8_t)incomingBuffer[17]);
            _receivedMessage.cell_7 = ((((uint8_t)incomingBuffer[18]) << 8) + (uint8_t)incomingBuffer[19]);
            _receivedMessage.cell_8 = ((((uint8_t)incomingBuffer[20]) << 8) + (uint8_t)incomingBuffer[21]);
            _receivedMessage.cell_9 = ((((uint8_t)incomingBuffer[22]) << 8) + (uint8_t)incomingBuffer[23]);
            _receivedMessage.cell_10 = ((((uint8_t)incomingBuffer[24]) << 8) + (uint8_t)incomingBuffer[25]);
            _receivedMessage.cell_11 = ((((uint8_t)incomingBuffer[26]) << 8) + (uint8_t)incomingBuffer[27]);
            _receivedMessage.cell_12 = ((((uint8_t)incomingBuffer[28]) << 8) + (uint8_t)incomingBuffer[29]);
            _receivedMessage.cell_13 = ((((uint8_t)incomingBuffer[30]) << 8) + (uint8_t)incomingBuffer[31]);
            _receivedMessage.cell_14 = ((((uint8_t)incomingBuffer[32]) << 8) + (uint8_t)incomingBuffer[33]);
            _receivedMessage.cell_15 = ((((uint8_t)incomingBuffer[14]) << 8) + (uint8_t)incomingBuffer[35]);
            _receivedMessage.cell_16 = ((((uint8_t)incomingBuffer[16]) << 8) + (uint8_t)incomingBuffer[37]);
            _receivedMessage.cell_17 = ((((uint8_t)incomingBuffer[18]) << 8) + (uint8_t)incomingBuffer[39]);
            _receivedMessage.cell_18 = ((((uint8_t)incomingBuffer[20]) << 8) + (uint8_t)incomingBuffer[41]);
            _receivedMessage.cell_19 = ((((uint8_t)incomingBuffer[22]) << 8) + (uint8_t)incomingBuffer[43]);
            _receivedMessage.cell_20 = ((((uint8_t)incomingBuffer[24]) << 8) + (uint8_t)incomingBuffer[45]);
            _receivedMessage.cell_21 = ((((uint8_t)incomingBuffer[26]) << 8) + (uint8_t)incomingBuffer[47]);
            _receivedMessage.cell_22 = ((((uint8_t)incomingBuffer[28]) << 8) + (uint8_t)incomingBuffer[49]);
            _receivedMessage.cell_23 = ((((uint8_t)incomingBuffer[30]) << 8) + (uint8_t)incomingBuffer[51]);
            _receivedMessage.cell_24 = ((((uint8_t)incomingBuffer[32]) << 8) + (uint8_t)incomingBuffer[53]);
            _receivedMessage.Ah_install = ((((uint8_t)incomingBuffer[75]) << 24) + (((uint8_t)incomingBuffer[76]) << 16) + (((uint8_t)incomingBuffer[77]) << 8) + ((uint8_t)incomingBuffer[78]));
            _receivedMessage.Ah_rest = ((((uint8_t)incomingBuffer[79]) << 24) + (((uint8_t)incomingBuffer[80]) << 16) + (((uint8_t)incomingBuffer[81]) << 8) + ((uint8_t)incomingBuffer[82]));
            _receivedMessage.Ah_Total = ((((uint8_t)incomingBuffer[83]) << 24) + (((uint8_t)incomingBuffer[84]) << 16) + (((uint8_t)incomingBuffer[85]) << 8) + ((uint8_t)incomingBuffer[86]));
            */



            printValuesToSerialAndSendMQTT();
            sendCanMessage();


            // Invert LED
            digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);

        }
        else
        {
            // Not valid
            _invalidResponseCounter++;

            if (PRINT_SERIAL_AND_SEND_MQTT_ANYWAY)
            {
                printValuesToSerialAndSendMQTT();

                // Invert LED
                digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);
            }
        }
    }

    sprintf(_oledLine2, "%u", _validResponseCounter);
    sprintf(_oledLine4, "%u", _invalidResponseCounter);
    updateOLED("Valid", _oledLine2, "Invalid", _oledLine4);
}






/*
setupWifi

Connect to WiFi
*/
void setupWifi()
{
    // We start by connecting to a WiFi network
#ifdef DEBUG
    sprintf(_debugOutput, "Connecting to %s", WIFI_SSID);
    Serial.println(_debugOutput);
#endif

    // Set up in Station Mode - Will be connecting to an access point
    WiFi.mode(WIFI_STA);

    // And connect to the details defined at the top
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // And continually try to connect to WiFi.  If it doesn't, the device will just wait here before continuing
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        updateOLED("Connecting", "WiFi...", "", "");
    }

    // Set the hostname for this Arduino
    WiFi.hostname(DEVICE_NAME);

    // Output some debug information
#ifdef DEBUG
    Serial.print("WiFi connected, IP is");
    Serial.print(WiFi.localIP());
#endif

    // Connected, so ditch out with blank screen
    updateOLED("", "", "", "");
}



/*
emptyPayload

Clears every char so end of string can be easily found
*/
void emptyPayload()
{
    for (int i = 0; i < _maxPayloadSize; i++)
    {
        _mqttPayload[i] = '\0';
    }
}



/*
mqttCallback()

// This function is executed when an MQTT message arrives on a topic that we are subscribed to.
*/
void mqttCallback(char* topic, byte* message, unsigned int length)
{
    return;
}


/*
addToPayload()

Adds characters to the payload buffer
*/
void addToPayload(char* addition)
{
    int targetRequestedSize = strlen(_mqttPayload) + strlen(addition);

    // If max payload size is 2048 it is stored as (0-2047), however character 2048  (position 2047) is null terminator so 2047 chars usable usable
    if (targetRequestedSize > _maxPayloadSize - 1)
    {
        // Safely print using snprintf
        snprintf(_mqttPayload, _maxPayloadSize, "{\r\n    \"mqttError\": \"Length of payload exceeds %d bytes.  Length would be %d bytes.\"\r\n}", _maxPayloadSize - 1, targetRequestedSize);

        emptyPayload();
    }
    else
    {
        // Add to the payload by sprintf back on itself with the addition
        sprintf(_mqttPayload, "%s%s", _mqttPayload, addition);
    }
}




/*
mqttReconnect

This function reconnects the ESP8266 to the MQTT broker
*/
void mqttReconnect()
{
    bool subscribed = false;
    char subscriptionDef[100];

    // Loop until we're reconnected
    while (true)
    {

        _mqtt.disconnect();		// Just in case.
        delay(200);

#ifdef DEBUG
        Serial.print("Attempting MQTT connection...");
#endif

        updateOLED("Connecting", "MQTT...", "", "");
        delay(100);

        // Attempt to connect
        if (_mqtt.connect(DEVICE_NAME, MQTT_USERNAME, MQTT_PASSWORD))
        {
            Serial.println("Connected MQTT");
            break;
        }

        // Wait 5 seconds before retrying
        delay(5000);
    }
}




/*
sendMqtt

Sends whatever is in the modular level payload to the specified topic.
*/
void sendMqtt(char* topic)
{
    // Attempt a send
    if (!_mqtt.publish(topic, _mqttPayload))
    {
#ifdef DEBUG
        sprintf(_debugOutput, "MQTT publish failed to %s", topic);
        Serial.println(_debugOutput);
        Serial.println(_mqttPayload);
#endif
    }
    else
    {
#ifdef DEBUG
        //sprintf(_debugOutput, "MQTT publish success");
        //Serial.println(_debugOutput);
#endif
    }

    // Empty payload for next use.
    emptyPayload();
    return;
}






/*
checkCRC

Calculates the CRC for any given data frame and compares it to what came in
calcCRC is based on
https://github.com/angeloc/simplemodbusng/blob/master/SimpleModbusMaster/SimpleModbusMaster.cpp
*/
bool checkCRC(uint8_t frame[], byte actualFrameSize)
{
    unsigned int calculated_crc, received_crc;

    received_crc = ((frame[actualFrameSize - 2] << 8) | frame[actualFrameSize - 1]);
    calcCRC(frame, actualFrameSize);
    calculated_crc = ((frame[actualFrameSize - 2] << 8) | frame[actualFrameSize - 1]);

    return (received_crc == calculated_crc);
}


/*
calcCRC

Calculates the CRC for any given data frame
calcCRC is based on
https://github.com/angeloc/simplemodbusng/blob/master/SimpleModbusMaster/SimpleModbusMaster.cpp
*/
void calcCRC(uint8_t frame[], byte actualFrameSize)
{
    unsigned int temp = 0xffff, flag;

    for (unsigned char i = 0; i < actualFrameSize - 2; i++)
    {
        temp = temp ^ frame[i];

        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>= 1;

            if (flag)
                temp ^= 0xA001;
        }
    }

    // Bytes are reversed.
    frame[actualFrameSize - 2] = temp & 0xff;
    frame[actualFrameSize - 1] = temp >> 8;
}