/*

Copyright (C) 2023  "Daniel Young" <daniel@mydan.com>
License: GPL Version 3

*/

#define USE_MCP_CAN
//#define USE_WIFI_AND_MQTT


#ifdef USE_MCP_CAN
#include <SPI.h>
#include <mcp_can.h>
#endif


#ifdef USE_WIFI_AND_MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#endif


// User Parameters

// Set CS to GPIO5
#define MCP2515_CS 5
// Set INT to GPIO4 (Not needed, but coded in anyway)
#define MCP2515_INT 26

// If you don't want to poll the BMS and send to CAN unless a pin is given 3.3v, leave this as is
//#define USE_SHORTING_PIN
#define SHORTING_PIN 13

//2575
// "Charge Voltage Limit (CVL)" - This is multiplied by the number of cells in the BMS      (ChargeVsetpoint), i.e. 22 * 2575 = 56650 / 1000 = 56.65 V
#define CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS 2618

// "Charge Current Limit (CCL)" : max charge current in 0.1A (chargecurrent)  i.e. 400 = 40A
#define CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP 400

// "Discharge Current Limit (DCL)" : max discharge current in 0.1A (discurrent) i.e. 400 = 40A
#define DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP 400 

// "Max Cell Voltage" max cell voltage in mv (MaxCellVoltage)
// #define MAX_CELL_VOLTAGE_IN_MILLIVOLTS 2575

// "Discharge Voltage Limit (DVL)" = - This is multiplied by the number of cells in the BMS      (DischVsetpoint), i.e. 22 * 2000 = 44 V
// #define DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS 1900
#define DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS 1950

// Even if no BMS response.. send MQTT and print
#define PRINT_SERIAL_AND_SEND_MQTT_ANYWAY true

// Unsure if needed at this point
#define SUPPORTS_NEW_COMMANDS false

// For debugging, uses a fixed message customisable in readBms()
#define USE_FIXED_MESSAGE_FOR_DEBUGGING false


#ifdef USE_WIFI_AND_MQTT
// Update with Wifi details
/*
#define WIFI_SSID		"NoDeal"
#define WIFI_PASSWORD	"J6786yyy6786"

// Update with your MQTT Broker details
#define MQTT_SERVER	"192.168.1.253"
#define MQTT_PORT	1883
#define MQTT_USERNAME	"battery"			// Empty string for none.
#define MQTT_PASSWORD	"Switch1"
*/

#define WIFI_SSID		"Stardust"
#define WIFI_PASSWORD	""

// Update with your MQTT Broker details
#define MQTT_SERVER	"192.168.1.135"
#define MQTT_PORT	1883
#define MQTT_USERNAME	"Alpha"			// Empty string for none.
#define MQTT_PASSWORD	"Inverter1"


// The device name is used as the MQTT base topic and presence on the network.
// If you need more than one AntBmsToCan on your network, give them unique names.
#define DEVICE_NAME "AntBmsToCan"

// Status will be posted to the following topic
#define MQTT_BMS_DETAILS "/bms/details"


#define MQTT_BMS_NOTENOUGHDATA "/bms/notenoughdata"
#define MQTT_BMS_FAILEDCHECKSUM "/bms/failedchecksum"
#define MQTT_BMS_FAILEDHEADER "/bms/failedheader"
#define MQTT_CAN_FAILSEND "/can/failsend"
#define MQTT_CAN_TXERRORCOUNT "/can/txerrorcount"

#define BMS_ERRORTYPE_NOTENOUGHDATA 1
#define BMS_ERRORTYPE_FAILEDCHECKSUM 2
#define BMS_ERRORTYPE_FAILEDHEADER 3
#define CAN_ERRORTYPE_CANFAILSEND 4
#define CAN_ERRORTYPE_CANTXERRORCOUNT 5

#endif




// BMS RX and TX Pins
#define BMS_RX_PIN 16
#define BMS_TX_PIN 17


// These timers are used in the main loop.
// How frequently to poll BMS information and send via CAN
#define BMS_QUERY_INTERVAL 1000

// How long to wait before we give up waiting for a BMS response
#define BMS_TIMEOUT 250





// Fixed Parameters

// Baud Rates
#define BMS_BAUD_RATE 19200
#define ESP_DEBUGGING_BAUD_RATE 115200

#define KELVIN_OFFSET 273.15

// This is fixed by the AntBMS protocol
#define MAX_CELLS_SUPPORTED_BY_BMS 32

// This is fixed by the AntBMS protocol
#define MAX_TEMPERATURE_SENSORS_SUPPORTED_BY_BMS 6

// Positions of each temperature sensor reading in the array
#define TEMPERATURE_MOSFET 0
#define TEMPERATURE_BALANCER 1
#define TEMPERATURE_SENSOR_1 2
#define TEMPERATURE_SENSOR_2 3
#define TEMPERATURE_SENSOR_3 4
#define TEMPERATURE_SENSOR_4 5




// How long a BMS message is, this is fixed at 1400 as per the AntBMS protocol
#define BMS_MESSAGE_LENGTH 140





// And code onwards



HardwareSerial _bms(2); // UART 2 (GPIO16 RX2, GPIO17 TX2)

// Totals for racking up
unsigned long _bmsValidResponseCounter = 0;
unsigned long _bmsInvalidResponseCounter = 0;
unsigned long _canSuccessCounter = 0;
unsigned long _canFailureCounter = 0;




#ifdef USE_WIFI_AND_MQTT

// On boot will request a buffer size of (MAX_MQTT_PAYLOAD_SIZE + MQTT_HEADER_SIZE) for MQTT, and
// MAX_MQTT_PAYLOAD_SIZE for building payloads.  If these fail and your device doesn't boot, you can assume you've set this too high.
#define MAX_MQTT_PAYLOAD_SIZE 4096
#define MIN_MQTT_PAYLOAD_SIZE 512
#define MQTT_HEADER_SIZE 512

// How frequently to send a response over MQTT
#define MQTT_SEND_INTERVAL 10000


// WiFi parameters
WiFiClient _wifi;

// MQTT parameters
PubSubClient _mqtt(_wifi);

// Buffer Size (and therefore payload size calc)
int _bufferSize;
int _maxPayloadSize;

// I want to declare this once at a modular level, keep the heap somewhere in check.
char* _mqttPayload;

#endif


struct bmsResponse
{
    uint16_t rawTotalVoltage;
    float totalVoltage;
    uint8_t cells;
    float cellVoltage[MAX_CELLS_SUPPORTED_BY_BMS];
    uint32_t rawCurrent;
    float current;
    float rawCurrentAsFloat;
    uint8_t rawSoc;
    float soc;
    float totalBatteryCapacitySetting;
    float capacityRemaining;
    float batteryCycleCapacity;
    float totalRuntime;
    uint16_t rawTemperatureMosfet;
    float temperatures[MAX_TEMPERATURE_SENSORS_SUPPORTED_BY_BMS];
    uint8_t chargeMosfetStatus;
    bool chargingSwitch;
    uint8_t dischargeMosfetStatus;
    bool dischargingSwitch;
    uint8_t balancerStatus;
    bool balancerSwitch;
    float power;
    uint8_t cellWithHighestVoltage;
    uint16_t rawMaxCellVoltage;
    float maxCellVoltage;
    uint8_t cellWithLowestVoltage;
    uint16_t rawMinCellVoltage;
    float minCellVoltage;
    float deltaCellVoltage;
    float averageCellVoltage;
    uint8_t batteryStrings;
};
bmsResponse _receivedResponse;







#ifdef USE_MCP_CAN
MCP_CAN CAN0(MCP2515_CS);
#endif



#ifdef USE_MCP_CAN
void printCanResultToSerialMCP(unsigned long id, byte canResult)
{
    char msgString[128];

    sprintf(msgString, "0x%.8lX", id);

    if (id >= 0)
    {
        Serial.print("CAN Packet ");Serial.print(msgString);Serial.print(",  Result: ");Serial.print(canResult);Serial.print(", ");
    }
    else if (id < 0)
    {
        Serial.print("CAN Read Result ");
    }

    switch (canResult) {
    case CAN_OK: Serial.print("CAN_OK\r\n");
        break;
    case CAN_FAILINIT: Serial.print("CAN_FAILINIT\r\n");
        break;
    case CAN_FAILTX: Serial.print("CAN_FAILTX\r\n");
        break;
    case CAN_MSGAVAIL: Serial.print("CAN_MSGAVAIL\r\n");
        break;
    case CAN_NOMSG: Serial.print("CAN_NOMSG\r\n");
        break;
    case CAN_CTRLERROR: Serial.print("CAN_CTRLERROR\r\n");
        break;
    case CAN_GETTXBFTIMEOUT: Serial.print("CAN_GETTXBFTIMEOUT\r\n");
        break;
    case CAN_SENDMSGTIMEOUT: Serial.print("CAN_SENDMSGTIMEOUT\r\n");
        break;
    case CAN_FAIL: Serial.print("CAN_FAIL\r\n");
        break;
    }
    Serial.println("");
}
#endif


/*
Returns low byte of a uint16_t, due to not being sure about inbuilt lowByte function.
*/
uint8_t lowByteOfUint16T(uint16_t input)
{
    return input & 0xff;
}

/*
Returns high byte of a uint16_t, due to not being sure about inbuilt highByte function.
*/
uint8_t highByteOfUint16T(uint16_t input)
{
    return (input >> 8) & 0xff;
}


/*
calcChecksum()

Calculates the relevant checksum for the frame
*/
uint16_t calcChecksum(const uint8_t data[], const uint16_t len) {
    uint16_t checksum = 0;
    for (uint16_t i = 4; i < len; i++) {
        checksum = checksum + data[i];
    }
    return checksum;
}





/*
sendCanMessage()

Sends appropriately formatted messages based on the BMS details read in over to CAN following the
spec of Pylontech

https://www.setfirelabs.com/green-energy/pylontech-can-reading-can-replication

*/

bool sendCanMessage()
{
    uint16_t SOH = 100; // SOH place holder

    unsigned char alarmBuf[4] = { 0, 0, 0, 0 };
    unsigned char warningBuf[4] = { 0, 0, 0, 0 };
    //unsigned char mes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    // unsigned char bmsname[8] = { '@', 'A', 'n', 't', 'B', 'm', 's', '@' };
    unsigned char bmsnameBuf[8] = { 'P', 'Y', 'L', 'O', 'N', ' ', ' ', ' ' };

    bool result = true;


#ifdef USE_MCP_CAN
    uint8_t data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    byte canResult;
    // Serial Output String Buffer
    char msgString[128];
    unsigned long id;
    byte len;
    byte ext = 0;
    
    if (result)
    {
        id = 0x351;
        len = 8;
        ext = 0;

        // In the example, 2575 * 22 cells = 56650 / 100 = 566 tenths of a volt.
        data[0] = lowByteOfUint16T(uint16_t((CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedResponse.cells) / 100));
        data[1] = highByteOfUint16T(uint16_t((CHARGE_VOLTAGE_LIMIT_CVL_IN_MILLIVOLTS * _receivedResponse.cells) / 100));
        data[2] = lowByteOfUint16T(uint16_t(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // chargecurrent
        data[3] = highByteOfUint16T(uint16_t(CHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // chargecurrent
        data[4] = lowByteOfUint16T(uint16_t(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // dischargecurrent
        data[5] = highByteOfUint16T(uint16_t(DISCHARGE_CURRENT_LIMIT_IN_TENTHS_OF_AN_AMP)); // dischargecurrent

        // Debatable if these needed
        data[6] = lowByteOfUint16T(uint16_t((DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedResponse.cells) / 100));
        data[7] = highByteOfUint16T(uint16_t((DISCHARGE_VOLTAGE_LIMIT_DVL_IN_MILLIVOLTS * _receivedResponse.cells) / 100));

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }


    if (result)
    {
        delay(2); // or 2

        id = 0x355;
        len = 8;
        ext = 0;

        data[0] = lowByteOfUint16T(uint16_t(_receivedResponse.rawSoc)); //SOC
        data[1] = highByteOfUint16T(uint16_t(_receivedResponse.rawSoc)); //SOC
        data[2] = lowByteOfUint16T(uint16_t(SOH)); //SOH
        data[3] = highByteOfUint16T(uint16_t(SOH)); //SOH

        // Debatable if this is needed
        data[4] = lowByteOfUint16T(uint16_t(_receivedResponse.rawSoc * 10)); //SOC
        data[5] = highByteOfUint16T(uint16_t(_receivedResponse.rawSoc * 10)); //SOC
        data[6] = 0;
        data[7] = 0;

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");


        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }
    

    if (result)
    {
        delay(2);

        // Multiplies voltage by 10 to convert into millivolts.  This likely makes sense, in my example 262 tenths of a voltage is increased to 2620 millivolts
        // But that goes against everything else which is reported in tenths
        // Why is current made negative?
        // Temperature which comes in as actual degrees (14) is multiplied by 10, so pylontech reports in terms of tenths of a degree?

        // Voltage / Current / Temp - 6 bytes of data
        // 16 bits, signed int, 2s complement
        // V in 0.01V, A in 0.1A, T in 0.1C
        id = 0x356;
        len = 8;
        ext = 0;

        data[0] = lowByteOfUint16T(uint16_t(_receivedResponse.rawTotalVoltage * 10));//lowByte(uint16_t(_receivedResponse.rawTotalVoltage * 10));
        data[1] = highByteOfUint16T(uint16_t(_receivedResponse.rawTotalVoltage * 10));//highByte(uint16_t(_receivedResponse.rawTotalVoltage * 10));
        data[2] = lowByte(long(-_receivedResponse.rawCurrentAsFloat));// lowByte(long(-_receivedResponse.rawCurrent));
        data[3] = highByte(long(-_receivedResponse.rawCurrentAsFloat));//highByte(long(-_receivedResponse.rawCurrent));
        data[4] = lowByteOfUint16T(uint16_t(_receivedResponse.rawTemperatureMosfet * 10));//lowByte(uint16_t(_receivedResponse.rawTemperatureMosfet * 10));
        data[5] = highByteOfUint16T(uint16_t(_receivedResponse.rawTemperatureMosfet * 10));//highByte(uint16_t(_receivedResponse.rawTemperatureMosfet * 10));
        data[6] = 0;
        data[7] = 0;

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }

    

    if (result)
    {
        // Spec for Pylontech addresses this as 0x359
        delay(2);
        id = 0x359;
        len = 8;
        ext = 0;

        data[0] = alarmBuf[0];
        data[1] = alarmBuf[1];
        data[2] = alarmBuf[2];
        data[3] = alarmBuf[3];
        data[4] = warningBuf[0];
        data[5] = warningBuf[1];
        data[6] = warningBuf[2];
        data[7] = warningBuf[3];

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }



    if (result)
    {
        // Send the battery charge request packet even if empty?
        delay(2);
        id = 0x35C;
        len = 8;
        ext = 0;

        data[0] = 0;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;
        data[6] = 0;
        data[7] = 0;

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }

    if (result)
    {
        delay(2);
        id = 0x35E;
        len = 8;
        ext = 0;

        data[0] = bmsnameBuf[0];
        data[1] = bmsnameBuf[1];
        data[2] = bmsnameBuf[2];
        data[3] = bmsnameBuf[3];
        data[4] = bmsnameBuf[1];
        data[5] = bmsnameBuf[2];
        data[6] = bmsnameBuf[3];
        data[7] = bmsnameBuf[4];

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }
    

    /*
    if (result)
    {
        delay(2);
        id = 0x373;
        len = 8;
        ext = 0;

        data[0] = lowByteOfUint16T(_receivedResponse.rawMinCellVoltage);
        data[1] = highByteOfUint16T(_receivedResponse.rawMinCellVoltage);
        data[2] = lowByteOfUint16T(_receivedResponse.rawMaxCellVoltage);
        data[3] = highByteOfUint16T(_receivedResponse.rawMaxCellVoltage);
        data[4] = lowByteOfUint16T(uint16_t(0 + KELVIN_OFFSET));
        data[5] = highByteOfUint16T(uint16_t(0 + KELVIN_OFFSET));
        data[6] = lowByteOfUint16T(uint16_t(0 + KELVIN_OFFSET));
        data[7] = highByteOfUint16T(uint16_t(0 + KELVIN_OFFSET));

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }



    if (result)
    {
        delay(2);
        id = 0x372;
        len = 8;
        ext = 0;

        data[0] = _receivedResponse.cells;  // lowByteOfUint16T(_receivedResponse.cells);
        data[1] = 0; // highByteOfUint16T(_receivedResponse.cells);
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;
        data[6] = 0;
        data[7] = 0;

        sprintf(msgString, "SEND %s ID 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", id, len);
        Serial.print(msgString);

        for (byte i = 0; i < 8; i++) {
            sprintf(msgString, " 0x%.2X", data[i]);
            Serial.print(msgString);
        }
        Serial.println("");

        canResult = CAN0.sendMsgBuf(id, ext, len, data);

        result = (canResult == CAN_OK);
        if (canResult != CAN_OK)
        {
            printCanResultToSerialMCP(id, canResult);
        }
        else
        {
#ifdef USE_WIFI_AND_MQTT
            sendMqttState(CAN_ERRORTYPE_CANFAILSEND, 0, id, canResult, 0);
#endif
        }
    }
    */



    if (CAN0.errorCountTX() > 0)
    {
        Serial.print("errorCountTX: ");Serial.println(CAN0.errorCountTX());

#ifdef USE_WIFI_AND_MQTT
        sendMqttState(CAN_ERRORTYPE_CANTXERRORCOUNT, 0, 0, 0, CAN0.errorCountTX());
#endif

    }



#endif





    /*
  msg.id  = 0x351;
  msg.len = 8;
  msg.buf[0] = lowByte(uint16_t(ChargeVsetpoint * nbr_cell)/100);
  msg.buf[1] = highByte(uint16_t(ChargeVsetpoint * nbr_cell)/100);
  msg.buf[2] = lowByte(chargecurrent);//  chargecurrent
  msg.buf[3] = highByte(chargecurrent);//  chargecurrent
  msg.buf[4] = lowByte(discurrent );//discurrent
  msg.buf[5] = highByte(discurrent);//discurrent
  msg.buf[6] = lowByte(uint16_t(DischVsetpoint * nbr_cell)/100);
  msg.buf[7] = highByte(uint16_t(DischVsetpoint * nbr_cell)/100);
  VeCan.write(msg);

  delay(2);
  msg.id  = 0x355;
  msg.len = 8;
  msg.buf[0] = lowByte(soc);//SOC
  msg.buf[1] = highByte(soc);//SOC
  msg.buf[2] = lowByte(SOH);//SOH
  msg.buf[3] = highByte(SOH);//SOH
  msg.buf[4] = lowByte(soc * 10);//SOC
  msg.buf[5] = highByte(soc * 10);//SOC
  msg.buf[6] = 0;
  msg.buf[7] = 0;
  VeCan.write(msg);
//
  delay(2);
  msg.id  = 0x356;
  msg.len = 8;
  msg.buf[0] = lowByte(uint16_t(volt * 10));
  msg.buf[1] = highByte(uint16_t(volt * 10));
  msg.buf[2] = lowByte(long(-amp ));
  msg.buf[3] = highByte(long(-amp ));
  msg.buf[4] = lowByte(int16_t(t_fet * 10));
  msg.buf[5] = highByte(int16_t(t_fet * 10));
  msg.buf[6] = 0;
  msg.buf[7] = 0;
  VeCan.write(msg);
//
  delay(2);
  msg.id  = 0x35A;
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

  delay(2);
  msg.id  = 0x35E;
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

//  delay(2);
//  msg.id  = 0x370;
//  msg.len = 8;
//  msg.buf[0] = bmsmanu[0];
//  msg.buf[1] = bmsmanu[1];
//  msg.buf[2] = bmsmanu[2];
//  msg.buf[3] = bmsmanu[3];
//  msg.buf[4] = bmsmanu[4];
//  msg.buf[5] = bmsmanu[5];
//  msg.buf[6] = bmsmanu[6];
//  msg.buf[7] = bmsmanu[7];
//  VeCan.write(msg);

  delay(2);
  msg.id  = 0x373;
  msg.len = 8;
  msg.buf[0] = lowByte(uint16_t(cell_min));
  msg.buf[1] = highByte(uint16_t(cell_min));
  msg.buf[2] = lowByte(uint16_t(cell_max));
  msg.buf[3] = highByte(uint16_t(cell_max));
  msg.buf[4] = lowByte(uint16_t(LowTemp + 273.15));
  msg.buf[5] = highByte(uint16_t(LowTemp + 273.15));
  msg.buf[6] = lowByte(uint16_t(HighTemp + 273.15));
  msg.buf[7] = highByte(uint16_t(HighTemp + 273.15));
  VeCan.write(msg);

  delay(2);
  msg.id  = 0x372;
  msg.len = 8;
  msg.buf[0] = lowByte(nbr_cell);//bms.getNumModules()
  msg.buf[1] = highByte(nbr_cell);//bms.getNumModules()
  msg.buf[2] = 0x00;
  msg.buf[3] = 0x00;
  msg.buf[4] = 0x00;
  msg.buf[5] = 0x00;
  msg.buf[6] = 0x00;
  msg.buf[7] = 0x00;
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


    return result;
}



void printValuesToSerialAndSendToMQTTIfUsing()
{
    long milli_start = millis();

    Serial.println("--- Start Formatted ---");
    
    Serial.print("rawTotalVoltage = ");Serial.print(_receivedResponse.rawTotalVoltage, DEC);Serial.println("");
    Serial.print("rawCurrent = ");Serial.print(_receivedResponse.rawCurrent, DEC);Serial.println("");
    Serial.print("rawCurrentAsFloat = ");Serial.print(_receivedResponse.rawCurrentAsFloat, DEC);Serial.println("");
    Serial.print("rawSoc = ");Serial.print(_receivedResponse.rawSoc, DEC);Serial.println("");
    Serial.print("rawTemperatureMosfet = ");Serial.print(_receivedResponse.rawTemperatureMosfet, DEC);Serial.println("");
    Serial.print("rawMaxCellVoltage = ");Serial.print(_receivedResponse.rawMaxCellVoltage, DEC);Serial.println("");
    Serial.print("rawMinCellVoltage = ");Serial.print(_receivedResponse.rawMinCellVoltage, DEC);Serial.println("");



    Serial.print("totalVoltage = ");Serial.print(_receivedResponse.totalVoltage, DEC);Serial.println(" V");
    Serial.print("cells = ");Serial.print((_receivedResponse.cells), DEC);Serial.println("");
    Serial.print("cellVoltage[0] (1) = ");Serial.print(_receivedResponse.cellVoltage[0], DEC);Serial.println(" V");
    Serial.print("cellVoltage[1] (2) = ");Serial.print(_receivedResponse.cellVoltage[1], DEC);Serial.println(" V");
    Serial.print("cellVoltage[2] (3) = ");Serial.print(_receivedResponse.cellVoltage[2], DEC);Serial.println(" V");
    Serial.print("cellVoltage[3] (4) = ");Serial.print(_receivedResponse.cellVoltage[3], DEC);Serial.println(" V");
    Serial.print("cellVoltage[4] (5) = ");Serial.print(_receivedResponse.cellVoltage[4], DEC);Serial.println(" V");
    Serial.print("cellVoltage[5] (6) = ");Serial.print(_receivedResponse.cellVoltage[5], DEC);Serial.println(" V");
    Serial.print("cellVoltage[6] (7) = ");Serial.print(_receivedResponse.cellVoltage[6], DEC);Serial.println(" V");
    Serial.print("cellVoltage[7] (8) = ");Serial.print(_receivedResponse.cellVoltage[7], DEC);Serial.println(" V");
    Serial.print("cellVoltage[8] (9) = ");Serial.print(_receivedResponse.cellVoltage[8], DEC);Serial.println(" V");
    Serial.print("cellVoltage[9] (10) = ");Serial.print(_receivedResponse.cellVoltage[9], DEC);Serial.println(" V");
    Serial.print("cellVoltage[10] (11) = ");Serial.print(_receivedResponse.cellVoltage[10], DEC);Serial.println(" V");
    Serial.print("cellVoltage[11] (12) = ");Serial.print(_receivedResponse.cellVoltage[11], DEC);Serial.println(" V");
    Serial.print("cellVoltage[12] (13) = ");Serial.print(_receivedResponse.cellVoltage[12], DEC);Serial.println(" V");
    Serial.print("cellVoltage[13] (14) = ");Serial.print(_receivedResponse.cellVoltage[13], DEC);Serial.println(" V");
    Serial.print("cellVoltage[14] (15) = ");Serial.print(_receivedResponse.cellVoltage[14], DEC);Serial.println(" V");
    Serial.print("cellVoltage[15] (16) = ");Serial.print(_receivedResponse.cellVoltage[15], DEC);Serial.println(" V");
    Serial.print("cellVoltage[16] (17) = ");Serial.print(_receivedResponse.cellVoltage[16], DEC);Serial.println(" V");
    Serial.print("cellVoltage[17] (18) = ");Serial.print(_receivedResponse.cellVoltage[17], DEC);Serial.println(" V");
    Serial.print("cellVoltage[18] (19) = ");Serial.print(_receivedResponse.cellVoltage[18], DEC);Serial.println(" V");
    Serial.print("cellVoltage[19] (20) = ");Serial.print(_receivedResponse.cellVoltage[19], DEC);Serial.println(" V");
    Serial.print("cellVoltage[20] (21) = ");Serial.print(_receivedResponse.cellVoltage[20], DEC);Serial.println(" V");
    Serial.print("cellVoltage[21] (22) = ");Serial.print(_receivedResponse.cellVoltage[21], DEC);Serial.println(" V");
    Serial.print("cellVoltage[22] (23) = ");Serial.print(_receivedResponse.cellVoltage[22], DEC);Serial.println(" V");
    Serial.print("cellVoltage[23] (24) = ");Serial.print(_receivedResponse.cellVoltage[23], DEC);Serial.println(" V");
    Serial.print("cellVoltage[24] (25) = ");Serial.print(_receivedResponse.cellVoltage[24], DEC);Serial.println(" V");
    Serial.print("cellVoltage[25] (26) = ");Serial.print(_receivedResponse.cellVoltage[25], DEC);Serial.println(" V");
    Serial.print("cellVoltage[26] (27) = ");Serial.print(_receivedResponse.cellVoltage[26], DEC);Serial.println(" V");
    Serial.print("cellVoltage[27] (28) = ");Serial.print(_receivedResponse.cellVoltage[27], DEC);Serial.println(" V");
    Serial.print("cellVoltage[28] (29) = ");Serial.print(_receivedResponse.cellVoltage[28], DEC);Serial.println(" V");
    Serial.print("cellVoltage[29] (30) = ");Serial.print(_receivedResponse.cellVoltage[29], DEC);Serial.println(" V");
    Serial.print("cellVoltage[30] (31) = ");Serial.print(_receivedResponse.cellVoltage[30], DEC);Serial.println(" V");
    Serial.print("cellVoltage[31] (32) = ");Serial.print(_receivedResponse.cellVoltage[31], DEC);Serial.println(" V");
    Serial.print("current = ");Serial.print(_receivedResponse.current, DEC);Serial.println(" A");
    Serial.print("soc = ");Serial.print(_receivedResponse.soc, DEC);Serial.println(" %");
    Serial.print("totalBatteryCapacitySetting = ");Serial.print(_receivedResponse.totalBatteryCapacitySetting, DEC);Serial.println(" Ah");
    Serial.print("capacityRemaining = ");Serial.print(_receivedResponse.capacityRemaining, DEC);Serial.println(" Ah");
    Serial.print("batteryCycleCapacity = ");Serial.print(_receivedResponse.batteryCycleCapacity, DEC);Serial.println(" Ah");
    Serial.print("totalRuntime = ");Serial.print(_receivedResponse.batteryCycleCapacity, DEC);Serial.println(" s");
    Serial.print("temperatures[0] (MOSFET) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_MOSFET], DEC);Serial.println(" C");
    Serial.print("temperatures[1] (BALANCER) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_BALANCER], DEC);Serial.println(" C");
    Serial.print("temperatures[2] (1) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_SENSOR_1], DEC);Serial.println(" C");
    Serial.print("temperatures[3] (2) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_SENSOR_2], DEC);Serial.println(" C");
    Serial.print("temperatures[4] (3) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_SENSOR_3], DEC);Serial.println(" C");
    Serial.print("temperatures[5] (4) = ");Serial.print(_receivedResponse.temperatures[TEMPERATURE_SENSOR_4], DEC);Serial.println(" C");
    Serial.print("chargeMosfetStatus = ");Serial.print(_receivedResponse.chargeMosfetStatus, DEC);Serial.println("");
    Serial.print("chargingSwitch = ");Serial.print(_receivedResponse.chargingSwitch, DEC);Serial.println(" 1/0");
    Serial.print("dischargeMosfetStatus = ");Serial.print(_receivedResponse.dischargeMosfetStatus, DEC);Serial.println("");
    Serial.print("dischargingSwitch = ");Serial.print(_receivedResponse.dischargingSwitch, DEC);Serial.println(" 1/0");
    Serial.print("balancerStatus = ");Serial.print(_receivedResponse.balancerStatus, DEC);Serial.println("");
    Serial.print("balancerSwitch = ");Serial.print(_receivedResponse.balancerSwitch, DEC);Serial.println(" 1/0");
    Serial.print("power = ");Serial.print(_receivedResponse.power, DEC);Serial.println(" W");
    Serial.print("cellWithHighestVoltage = ");Serial.print(_receivedResponse.cellWithHighestVoltage, DEC);Serial.println("");
    Serial.print("maxCellVoltage = ");Serial.print(_receivedResponse.maxCellVoltage, DEC);Serial.println(" V");
    Serial.print("cellWithLowestVoltage = ");Serial.print(_receivedResponse.cellWithLowestVoltage, DEC);Serial.println("");
    Serial.print("minCellVoltage = ");Serial.print(_receivedResponse.minCellVoltage, DEC);Serial.println(" V");
    Serial.print("deltaCellVoltage = ");Serial.print(_receivedResponse.deltaCellVoltage, DEC);Serial.println(" V");
    Serial.print("averageCellVoltage = ");Serial.print(_receivedResponse.averageCellVoltage, DEC);Serial.println(" V");
    Serial.print("batteryStrings = ");Serial.print(_receivedResponse.batteryStrings, DEC);Serial.println("");


    Serial.print("Valid Responses = ");Serial.println(_bmsValidResponseCounter);
    Serial.print("Invalid Responses = ");Serial.println(_bmsInvalidResponseCounter);
    Serial.print("Ms = ");Serial.println((millis()) - (milli_start));
    Serial.println("--- End Formatted ---");


#ifdef USE_WIFI_AND_MQTT
    static unsigned long lastMqttSent = 0;

    char topicResponse[100] = ""; // 100 should cover a topic name
    char stateAddition[1024] = ""; // 256 should cover individual additions to be added to the payload.

    strcpy(topicResponse, DEVICE_NAME MQTT_BMS_DETAILS);

    // Time to query BMS?
    if (checkTimer(&lastMqttSent, MQTT_SEND_INTERVAL))
    {
        addToPayload("{\r\n");

        sprintf(stateAddition, "    \"rawTotalVoltage\": %d,\r\n", _receivedResponse.rawTotalVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawCurrent\": %d,\r\n", _receivedResponse.rawCurrent);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawCurrentAsFloat\": %d,\r\n", _receivedResponse.rawCurrentAsFloat);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawSoc\": %d,\r\n", _receivedResponse.rawSoc);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawTemperatureMosfet\": %d,\r\n", _receivedResponse.rawTemperatureMosfet);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawMaxCellVoltage\": %d,\r\n", _receivedResponse.rawMaxCellVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"rawMinCellVoltage\": %d,\r\n", _receivedResponse.rawMinCellVoltage);addToPayload(stateAddition);

        sprintf(stateAddition, "    \"totalVoltage\": %f,\r\n", _receivedResponse.totalVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cells\": %d,\r\n", _receivedResponse.cells);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage1\": %f,\r\n", _receivedResponse.cellVoltage[0]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage2\": %f,\r\n", _receivedResponse.cellVoltage[1]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage3\": %f,\r\n", _receivedResponse.cellVoltage[2]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage4\": %f,\r\n", _receivedResponse.cellVoltage[3]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage5\": %f,\r\n", _receivedResponse.cellVoltage[4]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage6\": %f,\r\n", _receivedResponse.cellVoltage[5]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage7\": %f,\r\n", _receivedResponse.cellVoltage[6]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage8\": %f,\r\n", _receivedResponse.cellVoltage[7]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage9\": %f,\r\n", _receivedResponse.cellVoltage[8]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage10\": %f,\r\n", _receivedResponse.cellVoltage[9]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage11\": %f,\r\n", _receivedResponse.cellVoltage[10]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage12\": %f,\r\n", _receivedResponse.cellVoltage[11]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage13\": %f,\r\n", _receivedResponse.cellVoltage[12]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage14\": %f,\r\n", _receivedResponse.cellVoltage[13]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage15\": %f,\r\n", _receivedResponse.cellVoltage[14]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage16\": %f,\r\n", _receivedResponse.cellVoltage[15]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage17\": %f,\r\n", _receivedResponse.cellVoltage[16]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage18\": %f,\r\n", _receivedResponse.cellVoltage[17]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage19\": %f,\r\n", _receivedResponse.cellVoltage[18]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage20\": %f,\r\n", _receivedResponse.cellVoltage[19]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage21\": %f,\r\n", _receivedResponse.cellVoltage[20]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage22\": %f,\r\n", _receivedResponse.cellVoltage[21]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage23\": %f,\r\n", _receivedResponse.cellVoltage[22]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage24\": %f,\r\n", _receivedResponse.cellVoltage[23]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage25\": %f,\r\n", _receivedResponse.cellVoltage[24]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage26\": %f,\r\n", _receivedResponse.cellVoltage[25]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage27\": %f,\r\n", _receivedResponse.cellVoltage[26]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage28\": %f,\r\n", _receivedResponse.cellVoltage[27]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage29\": %f,\r\n", _receivedResponse.cellVoltage[28]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage30\": %f,\r\n", _receivedResponse.cellVoltage[29]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage31\": %f,\r\n", _receivedResponse.cellVoltage[30]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellVoltage32\": %f,\r\n", _receivedResponse.cellVoltage[31]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"current\": %f,\r\n", _receivedResponse.current);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"soc\": %d,\r\n", _receivedResponse.soc);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"totalBatteryCapacitySetting\": %f,\r\n", _receivedResponse.totalBatteryCapacitySetting);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"capacityRemaining\": %f,\r\n", _receivedResponse.capacityRemaining);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"batteryCycleCapacity\": %f,\r\n", _receivedResponse.batteryCycleCapacity);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"totalRuntime\": %f,\r\n", _receivedResponse.totalRuntime);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures1Mosfet\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_MOSFET]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures2Balancer\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_BALANCER]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures3Sensor1\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_SENSOR_1]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures4Sensor2\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_SENSOR_2]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures5Sensor3\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_SENSOR_3]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"temperatures6Sensor4\": %f,\r\n", _receivedResponse.temperatures[TEMPERATURE_SENSOR_4]);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"chargeMosfetStatus\": %d,\r\n", _receivedResponse.chargeMosfetStatus);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"chargingSwitch\": %d,\r\n", _receivedResponse.chargingSwitch);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"dischargeMosfetStatus\": %d,\r\n", _receivedResponse.dischargeMosfetStatus);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"dischargingSwitch\": %d,\r\n", _receivedResponse.dischargingSwitch);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"balancerStatus\": %d,\r\n", _receivedResponse.balancerStatus);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"balancerSwitch\": %d,\r\n", _receivedResponse.balancerSwitch);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"power\": %f,\r\n", _receivedResponse.power);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellWithHighestVoltage\": %d,\r\n", _receivedResponse.cellWithHighestVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"maxCellVoltage\": %f,\r\n", _receivedResponse.maxCellVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"cellWithLowestVoltage\": %d,\r\n", _receivedResponse.cellWithLowestVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"minCellVoltage\": %f,\r\n", _receivedResponse.minCellVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"deltaCellVoltage\": %f,\r\n", _receivedResponse.deltaCellVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"averageCellVoltage\": %f,\r\n", _receivedResponse.averageCellVoltage);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"batteryStrings\": %d,\r\n", _receivedResponse.batteryStrings);addToPayload(stateAddition);

        sprintf(stateAddition, "    \"_bmsValidResponseCounter\": %d,\r\n", _bmsValidResponseCounter);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"_bmsInvalidResponseCounter\": %d,\r\n", _bmsInvalidResponseCounter);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"_canSuccessCounter\": %d,\r\n", _canSuccessCounter);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"_canFailureCounter\": %d\r\n", _canFailureCounter);addToPayload(stateAddition);

        addToPayload("}");
        sendMqtt(topicResponse);
    }
#endif

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

    // LED Output
    pinMode(LED_BUILTIN, OUTPUT);


#ifdef USE_WIFI_AND_MQTT
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
#endif

#ifdef USE_MCP_CAN

    byte errorCode;
    errorCode = CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);

    // Trigger input
#ifdef USE_SHORTING_PIN
    pinMode(SHORTING_PIN, INPUT_PULLDOWN);
    digitalWrite(SHORTING_PIN, LOW);
#endif


    while (errorCode != CAN_OK)
    {
        Serial.print("CAN configuration error: ");
        Serial.println(errorCode, HEX);
        delay(500);
        errorCode = CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
    }

    if (errorCode == 0) {

        // One shot transmission
        CAN0.enOneShotTX();

        // Since we do not set NORMAL mode, we are in loopback mode by default.
        CAN0.setMode(MCP_NORMAL);

        pinMode(MCP2515_INT, INPUT_PULLUP); // Configuring pin for /INT input

        Serial.println("MCP2515 Initialized Successfully!");

        delay(500);
    }



#endif


    return;
}



void loop()
{
    static unsigned long lastBmsQuery = 0;
    int shortingPinValue;

#ifdef USE_WIFI_AND_MQTT

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
#endif

    // Time to query BMS?  Works based on time elapsed and optionally, a shorting pin given 3.3V
#ifdef USE_SHORTING_PIN
    shortingPinValue = digitalRead(SHORTING_PIN);
#else
    shortingPinValue = HIGH;
#endif

    if (checkTimer(&lastBmsQuery, BMS_QUERY_INTERVAL) && shortingPinValue == HIGH)
    {
        readBms();
    }
    
    // And check for incoming messages - Not required but useful for debugging nonetheless
#ifdef USE_MCP_CAN
    checkReceiveMCPCAN();
#endif

    delay(10);
}



#ifdef USE_MCP_CAN
/*
Checks for incoming messages from the inverter... Does nothing apart from dump to serial
*/
void checkReceiveMCPCAN()
{

    byte canResult;

    // CAN RX Variables
    long unsigned int rxId;
    unsigned char len;
    unsigned char ext;
    int msgCount = 0;
    uint8_t rxBuf[8];

    // Serial Output String Buffer
    char msgString[128];
 
    
    if (CAN0.checkReceive() == CAN_MSGAVAIL)
    {
        //Serial.println(digitalRead(MCP2515_INT));

        canResult = CAN0.readMsgBuf(&rxId, &ext, &len, rxBuf);
        while (canResult == CAN_OK)
        {
            msgCount++;
            sprintf(msgString, "%s ID: 0x%.8lX  DLC: %1d  Data:", ext == 1 ? "Extended" : "Standard", (rxId & 0x1FFFFFFF), len);
            Serial.print(msgString);

            if ((rxId & 0x40000000) == 0x40000000)
            {
                // Determine if message is a remote request frame.
                sprintf(msgString, " REMOTE REQUEST FRAME");
                Serial.print(msgString);
            }
            else
            {
                for (byte i = 0; i < len; i++)
                {
                    sprintf(msgString, " 0x%.2X", rxBuf[i]);
                    Serial.print(msgString);
                }
            }

            Serial.println();
            canResult = CAN0.readMsgBuf(&rxId, &len, rxBuf);
        }

        if (canResult != CAN_OK && canResult != CAN_NOMSG)
        {
            printCanResultToSerialMCP(-1, canResult);
        }
        
        if (msgCount > 1)
        {
            // End of a batch, give a space.
            Serial.println();
        }

        /*
        for (int i = 0; i <= 3; i++)
        {
            delay(25);

            // Invert LED
            digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);
        }
        */
    }

    return;
}
#endif




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



/*
Grabs data from the BMS and onward posts to CAN
*/
void readBms()
{
    bool goodCrc = false;
   // bool canResult = false;
    bool goodHeader = true;

    // AntBms Request Data Command
    const byte requestCommand[] = { 0x5A, 0x5A, 0x00, 0x00, 0x01, 0x01 };

    // Received Messages begin with this
    const byte startMark[] = { 0xAA, 0x55, 0xAA, 0xFF };

    // Buffer for incoming data
    byte incomingBuffer[BMS_MESSAGE_LENGTH] = { 0 };

  //byte fixedTestMessage[BMS_MESSAGE_LENGTH] = {0xAA, 0x55, 0xAA, 0xFF, 0x01, 0x06, 0x0C, 0xCF, 0x0C, 0xC9, 0x0C, 0xCB, 0x0C, 0xCF, 0x0C, 0xCF, 0x0C, 0xD0, 0x0C, 0xCF, 0x0C, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFA, 0xF0, 0x80, 0x00, 0x0B, 0xEB, 0xC2, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0xE8, 0x28, 0x00, 0x0E, 0x00, 0x0D, 0x00, 0x0C, 0x00, 0x0A, 0xFF, 0xD8, 0xFF, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x0F, 0x06, 0x0C, 0xD0, 0x10, 0x0C, 0xC3, 0x0C, 0xCC, 0x08, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x02, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0xD3};
    byte fixedTestMessage[BMS_MESSAGE_LENGTH] = {0xAA, 0x55, 0xAA, 0xFF, 0x02, 0x30, 0x09, 0xE4, 0x09, 0xE5, 0x09, 0xE5, 0x09, 0xE4, 0x09, 0xE6, 0x09, 0xE6, 0x09, 0xC4, 0x09, 0xE8, 0x09, 0xE8, 0x09, 0xE9, 0x09, 0xE8, 0x09, 0xE9, 0x09, 0xFE, 0x0A, 0x0B, 0x0A, 0x05, 0x0A, 0x09, 0x0A, 0x06, 0x0A, 0x0D, 0x09, 0xDE, 0x0A, 0x0A, 0x0A, 0x04, 0x0A, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x63, 0x07, 0x27, 0x0E, 0x00, 0x06, 0xF4, 0xFA, 0x25, 0x00, 0xE8, 0xAF, 0xE2, 0x01, 0xFF, 0xC9, 0x8B, 0x00, 0x14, 0x00, 0x13, 0x00, 0x11, 0x00, 0x11, 0x00, 0x12, 0x00, 0x12, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xF0, 0x12, 0x0A, 0x0D, 0x07, 0x09, 0xC4, 0x09, 0xF1, 0x16, 0xFF, 0xFF, 0x00, 0x7E, 0x00, 0x7A, 0x02, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x8D};

    size_t bytesReceived;

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

 
    // Clear out outbound and inbound buffers
    _bms.flush();
    while (_bms.available())
    {
        _bms.read();
    }

    // Send the request command
    _bms.write(requestCommand, sizeof(requestCommand)); // while (Serial2.available()) {  Serial.print(Serial2.read()); };
    _bms.flush(); // Ensure sent on its way

    bytesReceived = _bms.readBytes(incomingBuffer, BMS_MESSAGE_LENGTH); // Will read up to 140 chars or timeout


    // If using a fixed text message, this will overwrite whatever came in from the serial with the fixed array declared above
    if (USE_FIXED_MESSAGE_FOR_DEBUGGING)
    {
        memcpy(incomingBuffer, fixedTestMessage, sizeof(incomingBuffer));
    }


    Serial.println("--- Start Raw Bytes ---");
    for (uint8_t i = 0; i < sizeof(incomingBuffer); i++)
    {
        sprintf(hexChar, "%02X", incomingBuffer[i]);
        Serial.print(hexChar);
        if (i < sizeof(incomingBuffer) - 1)
        {
            Serial.print(" ");
        }
    }
    Serial.println("");
    Serial.println("--- End Raw Bytes ---");


    if (bytesReceived != 140)
    {
#ifdef USE_WIFI_AND_MQTT
        // Send an MQTT saying data not received
        sendMqttState(BMS_ERRORTYPE_NOTENOUGHDATA, bytesReceived, 0, 0, 0);
#endif

    }


    // Obtain CRC
    uint16_t computedCrc = calcChecksum(incomingBuffer, BMS_MESSAGE_LENGTH - 2);
    uint16_t remoteCrc = uint16_t(incomingBuffer[BMS_MESSAGE_LENGTH - 2]) << 8 | (uint16_t(incomingBuffer[BMS_MESSAGE_LENGTH - 1]) << 0);

    goodCrc = (computedCrc == remoteCrc);
    if (!goodCrc)
    {
        // CRC Failed
        _bmsInvalidResponseCounter++;
#ifdef USE_WIFI_AND_MQTT
        // Send an MQTT saying failed checksum
        sendMqttState(BMS_ERRORTYPE_FAILEDCHECKSUM, 0, 0, 0, 0);
#endif
        /*
        if (PRINT_SERIAL_AND_SEND_MQTT_ANYWAY)
        {
            printValuesToSerialAndSendToMQTTIfUsing();
        }
        */
    }
    else if(!(incomingBuffer[0] == startMark[0] && incomingBuffer[1] == startMark[1] && incomingBuffer[2] == startMark[2] && incomingBuffer[3] == startMark[3]))
    {
        // Header failed
        _bmsInvalidResponseCounter++;

        goodHeader = false;
#ifdef USE_WIFI_AND_MQTT
        // Send an MQTT saying failed header
        sendMqttState(BMS_ERRORTYPE_FAILEDHEADER, 0, 0, 0, 0);
#endif

        /*
        if (PRINT_SERIAL_AND_SEND_MQTT_ANYWAY)
        {
            printValuesToSerialAndSendToMQTTIfUsing();
        }
        */
    }
    else
    {
        // Valid start and CRC, lets get the rest
        _bmsValidResponseCounter++;


        // Status response
        // <- 0xAA 0x55 0xAA 0xFF: Header
        //      0    1    2    3
        // *Data*
        //
        // Byte   Address Content: Description      Decoded content                         Coeff./Unit

        //   4    0x02 0x1D: Total voltage         541 * 0.1 = 54.1V                        0.1 V
        _receivedResponse.rawTotalVoltage = ant_get_16bit(4);
        float totalVoltage = ant_get_16bit(4) * 0.1f;
        _receivedResponse.totalVoltage = totalVoltage; // float

        //   6    0x10 0x2A: Cell voltage 1        4138 * 0.001 = 4.138V                    0.001 V
        //   8    0x10 0x2A: Cell voltage 2                4138 * 0.001 = 4.138V            0.001 V
        //  10    0x10 0x27: Cell voltage 3                4135 * 0.001 = 4.135V            0.001 V
        //  12    0x10 0x2A: Cell voltage 4                                                 0.001 V
        //  ...
        //  ...
        //  66    0x00 0x00: Cell voltage 31                                                0.001 V
        //  68    0x00 0x00: Cell voltage 32                                                0.001 V
        uint8_t cells = incomingBuffer[123];
        _receivedResponse.cells = cells; // uint8_t

        for (uint8_t i = 0; i < cells; i++)
        {
            _receivedResponse.cellVoltage[i] = (float)ant_get_16bit(i * 2 + 6) * 0.001f; // float
        }

        //  70    0x00 0x00 0x00 0x00: Current               0.0 A                          0.1 A
        _receivedResponse.rawCurrent = ant_get_32bit(70);
        float current = ((int32_t)ant_get_32bit(70)) * 0.1f;
        _receivedResponse.current = current; // float

        if (_receivedResponse.rawCurrent > 2147483648)
        {
            _receivedResponse.rawCurrentAsFloat = (-(2 * 2147483648) + _receivedResponse.rawCurrent);
        }
        else
        {
            _receivedResponse.rawCurrentAsFloat = _receivedResponse.rawCurrent;
        }


        //  74    0x64: SOC                                  100 %                          1.0 %
        _receivedResponse.soc = (float)incomingBuffer[74]; // uint8_t probably
        _receivedResponse.rawSoc = (uint8_t)incomingBuffer[74];

        //  75    0x02 0x53 0x17 0xC0: Total Battery Capacity Setting   39000000            0.000001 Ah
        _receivedResponse.totalBatteryCapacitySetting = (float)ant_get_32bit(75) * 0.000001f; // float

        //  79    0x02 0x53 0x06 0x11: Battery Capacity Remaining                           0.000001 Ah
        _receivedResponse.capacityRemaining = (float)ant_get_32bit(79) * 0.000001f; // float

        //  83    0x00 0x08 0xC7 0x8E: Battery Cycle Capacity                               0.001 Ah
        _receivedResponse.batteryCycleCapacity = (float)ant_get_32bit(83) * 0.001f; // float

        //  87    0x00 0x08 0x57 0x20: Uptime in seconds     546.592s                       1.0 s
        _receivedResponse.totalRuntime = (float)ant_get_32bit(87); // float

        //  91    0x00 0x15: Temperature 1                   21 C                           1.0  C
        //  93    0x00 0x15: Temperature 2                   21 C                           1.0  C
        //  95    0x00 0x14: Temperature 3                   20 C                           1.0  C
        //  97    0x00 0x14: Temperature 4                   20 C                           1.0  C
        //  99    0x00 0x14: Temperature 5                   20 C                           1.0  C
        //  101   0x00 0x14: Temperature 6                   20 C                           1.0  C
        _receivedResponse.rawTemperatureMosfet = ant_get_16bit(91);
        for (uint8_t i = 0; i < 6; i++)
        {
            _receivedResponse.temperatures[i] = (float)((int16_t)ant_get_16bit(i * 2 + 91)); // float
        }

       
        //  103   0x01: Charge MOSFET Status
        uint8_t raw_chargeMosfetStatus = incomingBuffer[103];
        _receivedResponse.chargeMosfetStatus = raw_chargeMosfetStatus; // uint8_t

        /*
        if (raw_chargeMosfetStatus < chargeMosfetStatus_SIZE)
        {
            _receivedResponse.chargeMosfetStatus_text = chargeMosfetStatus[raw_chargeMosfetStatus]; // Char Pointer
        }
        else
        {
            _receivedResponse.chargeMosfetStatus_text = "Unknown"; // Char Pointer
        }
        */
        _receivedResponse.chargingSwitch = (bool)(raw_chargeMosfetStatus == 0x01); // bool


        //  104   0x01: Discharge MOSFET Status
        uint8_t raw_dischargeMosfetStatus = incomingBuffer[104];
        _receivedResponse.chargeMosfetStatus = raw_dischargeMosfetStatus; // uint8_t

        /*
        if (raw_dischargeMosfetStatus < DISchargeMosfetStatus_SIZE)
        {
            _receivedResponse.dischargeMosfetStatus_text = DISchargeMosfetStatus[raw_dischargeMosfetStatus]; // Char Pointer
        }
        else
        {
            _receivedResponse.dischargeMosfetStatus_text = "Unknown"; // Char Pointer
        }
        */
        _receivedResponse.dischargingSwitch = (bool)(raw_chargeMosfetStatus == 0x01); // bool

        //  105   0x00: Balancer Status
        uint8_t raw_balancerStatus = incomingBuffer[105];
        _receivedResponse.balancerStatus = raw_balancerStatus; // uint8_t
        _receivedResponse.balancerSwitch = (bool)(raw_balancerStatus == 0x04); // bool

        /*
        if (raw_balancerStatus < balancerStatus_SIZE)
        {
            _receivedResponse.balancerStatus_text = balancerStatus[raw_balancerStatus]; // Char Pointer
        }
        else
        {
            _receivedResponse.balancerStatus_text = "Unknown"; // Char Pointer
        }
        */

        //  106   0x03 0xE8: Tire length                                                    mm
        //  108   0x00 0x17: Number of pulses per week
        //  110   0x01: Relay switch
        //  111   0x00 0x00 0x00 0x00: Current power         0W                             1.0 W
        if (SUPPORTS_NEW_COMMANDS)
        {
            _receivedResponse.power = totalVoltage * current;
        }
        else
        {
            _receivedResponse.power = (float)(int32_t)ant_get_32bit(111); // float
        }

        //  115   0x0D: Cell with the highest voltage        Cell 13
        _receivedResponse.cellWithHighestVoltage = (uint8_t)incomingBuffer[115]; // uint8_t

        //  116   0x10 0x2C: Maximum cell voltage            4140 * 0.001 = 4.140V          0.001 V
        _receivedResponse.rawMaxCellVoltage = ant_get_16bit(116);
        float maxCellVoltage = _receivedResponse.rawMaxCellVoltage * 0.001f;
        _receivedResponse.maxCellVoltage = maxCellVoltage; // float

        //  118   0x09: Cell with the lowest voltage         Cell 9
        _receivedResponse.cellWithLowestVoltage = (uint8_t)incomingBuffer[118]; // uint8_t

        //  119   0x10 0x26: Minimum cell voltage            4134 * 0.001 = 4.134V          0.001 V
        _receivedResponse.rawMinCellVoltage = ant_get_16bit(119);
        float minCellVoltage = _receivedResponse.rawMinCellVoltage * 0.001f;
        _receivedResponse.minCellVoltage = minCellVoltage; // float

        _receivedResponse.deltaCellVoltage = maxCellVoltage - minCellVoltage; // float

        //  121   0x10 0x28: Average cell voltage            4136 * 0.001 = 4.136V          0.001 V
        _receivedResponse.averageCellVoltage = ant_get_16bit(121) * 0.001f; // float

        //  123   0x0D: Battery strings                      13
        _receivedResponse.batteryStrings = (uint8_t)incomingBuffer[123]; // uint8_t


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

        printValuesToSerialAndSendToMQTTIfUsing();


        const bool canResult = sendCanMessage();

        if (canResult)
        {
            _canSuccessCounter++;
        }
        else
        {
            _canFailureCounter++;
        }

    }

    // Invert LED
    digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);

    return;
}





#ifdef USE_WIFI_AND_MQTT
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


    // We can't have a lack of wifi stopping the device speaking between BMS and inverter
    // So try the connect and move on, even if failure.

    if (WiFi.status() == WL_CONNECTED)
    {
        // Set the hostname for this Arduino
        WiFi.hostname(DEVICE_NAME);

        // Output some debug information
#ifdef DEBUG
        Serial.print("WiFi connected, IP is");
        Serial.print(WiFi.localIP());
#endif
    }
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

This function reconnects the ESP32 to the MQTT broker
*/
void mqttReconnect()
{
    bool subscribed = false;
    char subscriptionDef[100];

    // Again, can't have the device stuck until MQTT established... so just try the once.
    // If it works, it works.
    // Just in case.
    _mqtt.disconnect();
    delay(500);


#ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
#endif

    // Attempt to connect
    if (_mqtt.connect(DEVICE_NAME, MQTT_USERNAME, MQTT_PASSWORD))
    {
        Serial.println("Connected MQTT");
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




void sendMqttState(int state, size_t bytesReceived, unsigned long id, byte canResult, unsigned long txErrorCount)
{
    char topicResponse[100] = ""; // 100 should cover a topic name
    char stateAddition[1024] = ""; // 256 should cover individual additions to be added to the payload.


    if (state == BMS_ERRORTYPE_NOTENOUGHDATA)
    {
        strcpy(topicResponse, DEVICE_NAME MQTT_BMS_NOTENOUGHDATA);

        addToPayload("{\r\n");
        sprintf(stateAddition, "    \"bytesReceived\": %d\r\n", bytesReceived);addToPayload(stateAddition);
        addToPayload("}");
    }
    else if (state == BMS_ERRORTYPE_FAILEDCHECKSUM)
    {
        strcpy(topicResponse, DEVICE_NAME MQTT_BMS_FAILEDCHECKSUM);
        addToPayload("{\r\n");
        addToPayload("}");
    }
    else if (state == BMS_ERRORTYPE_FAILEDHEADER)
    {
        strcpy(topicResponse, DEVICE_NAME MQTT_BMS_FAILEDHEADER);
        addToPayload("{\r\n");
        addToPayload("}");
    }
    else if (state == CAN_ERRORTYPE_CANFAILSEND)
    {
        strcpy(topicResponse, DEVICE_NAME MQTT_CAN_FAILSEND);
        addToPayload("{\r\n");
        sprintf(stateAddition, "    \"id\": 0x%.8lX\r\n", id);addToPayload(stateAddition);
        sprintf(stateAddition, "    \"canResult\": %d\r\n", canResult);addToPayload(stateAddition);
        addToPayload("}");
    }
    else if (state == CAN_ERRORTYPE_CANTXERRORCOUNT)
    {
        strcpy(topicResponse, DEVICE_NAME MQTT_CAN_TXERRORCOUNT);
        addToPayload("{\r\n");
        sprintf(stateAddition, "    \"txErrorCount\": %d\r\n", txErrorCount);addToPayload(stateAddition);
        addToPayload("}");
    }
    


    // Send the error on its way
    sendMqtt(topicResponse);
}



#endif
