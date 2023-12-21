

uint16_t  ChargeVsetpoint = 2575;//  "Charge Voltage Limit (CVL)" = ChargeVsetpoint*NbrCell.
uint16_t  chargecurrent = 400; //"Charge Current Limit (CCL)" : max charge current in 0.1A
uint16_t  discurrent = 400; //"Discharge Current Limit (DCL)" : max discharge current in 0.1A
uint16_t  MaxCellVoltage = 2575; //: max cell voltage in mv
uint16_t  DischVsetpoint = 2000;//  


/*
Copyright (C) 2020  "Ju@Workshop" <ju82@free.fr>
License: GPL Version 3
*/

//variables for VE can
#include <mcp_can.h>


// Set CS to GPIO5
#define MCP2515_CS 5
// Set INT to GPIO4 (Not needed, but coded in anyway
#define MCP2515_INT 26

// BMS RX and TX Pins
#define BMS_RX_PIN 16
#define BMS_TX_PIN 17

#define BMS_BAUD_RATE 19200
#define ESP_DEBUGGING_BAUD_RATE 115200

// These timers are used in the main loop.
// How frequently to poll BMS information and send via CAN
#define BMS_QUERY_INTERVAL 5000

// How long to wait before we give up waiting for a BMS response
#define BMS_TIMEOUT 250


HardwareSerial _bms(2); // UART 2 (GPIO16 RX2, GPIO17 TX2)
MCP_CAN VeCan(MCP2515_CS);


int  Rate_ms = 2500; //milliS refrech CAN
uint16_t SOH = 100; // SOH place holder
unsigned char alarmBuf[4] = { 0, 0, 0, 0 };
unsigned char warningBuf[4] = { 0, 0, 0, 0 };
unsigned char mesBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char bmsnameBuf[8] = { '@', 'A', 'n', 't', 'B', 'm', 's', '@' };

//variables for AntBms
byte cmd[] = { 0x5A, 0x5A, 0x00, 0x00, 0x01, 0x01 };
byte startMark[] = { 0xAA, 0x55, 0xAA, 0xFF };
#define DATA_LENGTH           140
byte incomingByte[DATA_LENGTH] = { 0 };
int nbr_cell, soc, cell_av, cell_min, cell_max, cell_1, cell_2, cell_3, cell_4, cell_5, cell_6, cell_7, cell_8,
cell_9, cell_10, cell_11, cell_12, cell_13, cell_14, cell_15, cell_16, cell_17, cell_18, cell_19, cell_20,
cell_21, cell_22, cell_23, cell_24, t_bal, t_fet, t_1, t_2, t_3, t_4, puiss, HighTemp, LowTemp;
float volt, amp, Ah_install, Ah_rest, Ah_Total;
long int trame_ok, trame_nok;

void VEcan()
{
    uint8_t buf[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned long id;
    byte len;
    byte ext = 0;
    byte result = 0;

    id = 0x351;
    len = 8;
    buf[0] = lowByte(uint16_t(ChargeVsetpoint * nbr_cell) / 100);
    buf[1] = highByte(uint16_t(ChargeVsetpoint * nbr_cell) / 100);
    buf[2] = lowByte(chargecurrent);//  chargecurrent
    buf[3] = highByte(chargecurrent);//  chargecurrent
    buf[4] = lowByte(discurrent);//discurrent
    buf[5] = highByte(discurrent);//discurrent
    buf[6] = lowByte(uint16_t(DischVsetpoint * nbr_cell) / 100);
    buf[7] = highByte(uint16_t(DischVsetpoint * nbr_cell) / 100);
    result = VeCan.sendMsgBuf(id, ext, len, buf);

    delay(2);
    id = 0x355;
    len = 8;
    buf[0] = lowByte(soc);//SOC
    buf[1] = highByte(soc);//SOC
    buf[2] = lowByte(SOH);//SOH
    buf[3] = highByte(SOH);//SOH
    buf[4] = lowByte(soc * 10);//SOC
    buf[5] = highByte(soc * 10);//SOC
    buf[6] = 0;
    buf[7] = 0;
    result = VeCan.sendMsgBuf(id, ext, len, buf);

    //
    delay(2);
    id = 0x356;
    len = 8;
    buf[0] = lowByte(uint16_t(volt * 10));
    buf[1] = highByte(uint16_t(volt * 10));
    buf[2] = lowByte(long(-amp));
    buf[3] = highByte(long(-amp));
    buf[4] = lowByte(int16_t(t_fet * 10));
    buf[5] = highByte(int16_t(t_fet * 10));
    buf[6] = 0;
    buf[7] = 0;
    result = VeCan.sendMsgBuf(id, ext, len, buf);

    //
    delay(2);
    id = 0x35A;
    len = 8;
    buf[0] = alarmBuf[0];
    buf[1] = alarmBuf[1];
    buf[2] = alarmBuf[2];
    buf[3] = alarmBuf[3];
    buf[4] = warningBuf[0];
    buf[5] = warningBuf[1];
    buf[6] = warningBuf[2];
    buf[7] = warningBuf[3];
    result = VeCan.sendMsgBuf(id, ext, len, buf);


    delay(2);
    id = 0x35E;
    len = 8;
    buf[0] = bmsnameBuf[0];
    buf[1] = bmsnameBuf[1];
    buf[2] = bmsnameBuf[2];
    buf[3] = bmsnameBuf[3];
    buf[4] = bmsnameBuf[4];
    buf[5] = bmsnameBuf[5];
    buf[6] = bmsnameBuf[6];
    buf[7] = bmsnameBuf[7];
    result = VeCan.sendMsgBuf(id, ext, len, buf);


    //  delay(2);
    //  id  = 0x370;
    //  len = 8;
    //  buf[0] = bmsmanu[0];
    //  buf[1] = bmsmanu[1];
    //  buf[2] = bmsmanu[2];
    //  buf[3] = bmsmanu[3];
    //  buf[4] = bmsmanu[4];
    //  buf[5] = bmsmanu[5];
    //  buf[6] = bmsmanu[6];
    //  buf[7] = bmsmanu[7];
    //  result = VeCan.sendMsgBuf(id, ext, len, buf);

    delay(2);
    id = 0x373;
    len = 8;
    buf[0] = lowByte(uint16_t(cell_min));
    buf[1] = highByte(uint16_t(cell_min));
    buf[2] = lowByte(uint16_t(cell_max));
    buf[3] = highByte(uint16_t(cell_max));
    buf[4] = lowByte(uint16_t(LowTemp + 273.15));
    buf[5] = highByte(uint16_t(LowTemp + 273.15));
    buf[6] = lowByte(uint16_t(HighTemp + 273.15));
    buf[7] = highByte(uint16_t(HighTemp + 273.15));
    result = VeCan.sendMsgBuf(id, ext, len, buf);

    delay(2);
    id = 0x372;
    len = 8;
    buf[0] = lowByte(nbr_cell);//bms.getNumModules()
    buf[1] = highByte(nbr_cell);//bms.getNumModules()
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x00;
    buf[6] = 0x00;
    buf[7] = 0x00;
    result = VeCan.sendMsgBuf(id, ext, len, buf);
}

void read_bms() {
    _bms.flush(); _bms.write(cmd, sizeof(cmd)); // while (_bms.available()) {  Serial.print(_bms.read()); };
    for (int i = 0; i < DATA_LENGTH; i++) { while (!_bms.available()); incomingByte[i] = _bms.read(); }

    if (incomingByte[0] == startMark[0] && incomingByte[1] == startMark[1] && incomingByte[2] == startMark[2] && incomingByte[3] == startMark[3]) {
        trame_ok += 1;//  Serial.println("trame_ok :");
        extract_value();//  for(int i = 0; i < DATA_LENGTH; i++) { Serial.print(incomingByte[i], HEX);}
        print_value();
        VEcan();
        digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == LOW ? HIGH : LOW);          // added to show loop() activity 
    }
    else { trame_nok += 1; }
}

void extract_value() {
    uint32_t tmp = ((((uint8_t)incomingByte[70]) << 24) + (((uint8_t)incomingByte[71]) << 16) + (((uint8_t)incomingByte[72]) << 8) + ((uint8_t)incomingByte[73]));
    if (tmp > 2147483648) { amp = (-(2 * 2147483648) + tmp); }
    else { amp = tmp; }
    uint32_t tmp2 = ((((uint8_t)incomingByte[111]) << 24) + (((uint8_t)incomingByte[112]) << 16) + (((uint8_t)incomingByte[113]) << 8) + ((uint8_t)incomingByte[114]));
    if (tmp2 > 2147483648) { puiss = (-(2 * 2147483648) + tmp2); }
    else { puiss = tmp2; }
    nbr_cell = (uint8_t)incomingByte[123];
    soc = (uint8_t)incomingByte[74];
    volt = ((((uint8_t)incomingByte[4]) << 8) + (uint8_t)incomingByte[5]);
    cell_av = ((((uint8_t)incomingByte[121]) << 8) + (uint8_t)incomingByte[122]);
    cell_min = ((((uint8_t)incomingByte[119]) << 8) + (uint8_t)incomingByte[120]);
    cell_max = ((((uint8_t)incomingByte[116]) << 8) + (uint8_t)incomingByte[117]);
    t_fet = ((((uint8_t)incomingByte[91]) << 8) + (uint8_t)incomingByte[92]);
    t_bal = ((((uint8_t)incomingByte[93]) << 8) + (uint8_t)incomingByte[94]);
    t_1 = ((((uint8_t)incomingByte[95]) << 8) + (uint8_t)incomingByte[96]);
    t_2 = ((((uint8_t)incomingByte[97]) << 8) + (uint8_t)incomingByte[98]);
    t_3 = ((((uint8_t)incomingByte[99]) << 8) + (uint8_t)incomingByte[100]);
    t_4 = ((((uint8_t)incomingByte[101]) << 8) + (uint8_t)incomingByte[102]);
    cell_1 = ((((uint8_t)incomingByte[6]) << 8) + (uint8_t)incomingByte[7]);
    cell_2 = ((((uint8_t)incomingByte[8]) << 8) + (uint8_t)incomingByte[9]);
    cell_3 = ((((uint8_t)incomingByte[10]) << 8) + (uint8_t)incomingByte[11]);
    cell_4 = ((((uint8_t)incomingByte[12]) << 8) + (uint8_t)incomingByte[13]);
    cell_5 = ((((uint8_t)incomingByte[14]) << 8) + (uint8_t)incomingByte[15]);
    cell_6 = ((((uint8_t)incomingByte[16]) << 8) + (uint8_t)incomingByte[17]);
    cell_7 = ((((uint8_t)incomingByte[18]) << 8) + (uint8_t)incomingByte[19]);
    cell_8 = ((((uint8_t)incomingByte[20]) << 8) + (uint8_t)incomingByte[21]);
    cell_9 = ((((uint8_t)incomingByte[22]) << 8) + (uint8_t)incomingByte[23]);
    cell_10 = ((((uint8_t)incomingByte[24]) << 8) + (uint8_t)incomingByte[25]);
    cell_11 = ((((uint8_t)incomingByte[26]) << 8) + (uint8_t)incomingByte[27]);
    cell_12 = ((((uint8_t)incomingByte[28]) << 8) + (uint8_t)incomingByte[29]);
    cell_13 = ((((uint8_t)incomingByte[30]) << 8) + (uint8_t)incomingByte[31]);
    cell_14 = ((((uint8_t)incomingByte[32]) << 8) + (uint8_t)incomingByte[33]);
    cell_15 = ((((uint8_t)incomingByte[14]) << 8) + (uint8_t)incomingByte[35]);
    cell_16 = ((((uint8_t)incomingByte[16]) << 8) + (uint8_t)incomingByte[37]);
    cell_17 = ((((uint8_t)incomingByte[18]) << 8) + (uint8_t)incomingByte[39]);
    cell_18 = ((((uint8_t)incomingByte[20]) << 8) + (uint8_t)incomingByte[41]);
    cell_19 = ((((uint8_t)incomingByte[22]) << 8) + (uint8_t)incomingByte[43]);
    cell_20 = ((((uint8_t)incomingByte[24]) << 8) + (uint8_t)incomingByte[45]);
    cell_21 = ((((uint8_t)incomingByte[26]) << 8) + (uint8_t)incomingByte[47]);
    cell_22 = ((((uint8_t)incomingByte[28]) << 8) + (uint8_t)incomingByte[49]);
    cell_23 = ((((uint8_t)incomingByte[30]) << 8) + (uint8_t)incomingByte[51]);
    cell_24 = ((((uint8_t)incomingByte[32]) << 8) + (uint8_t)incomingByte[53]);
    Ah_install = ((((uint8_t)incomingByte[75]) << 24) + (((uint8_t)incomingByte[76]) << 16) + (((uint8_t)incomingByte[77]) << 8) + ((uint8_t)incomingByte[78]));
    Ah_rest = ((((uint8_t)incomingByte[79]) << 24) + (((uint8_t)incomingByte[80]) << 16) + (((uint8_t)incomingByte[81]) << 8) + ((uint8_t)incomingByte[82]));
    Ah_Total = ((((uint8_t)incomingByte[83]) << 24) + (((uint8_t)incomingByte[84]) << 16) + (((uint8_t)incomingByte[85]) << 8) + ((uint8_t)incomingByte[86]));
}

void   print_value() {
    long int milli_start = millis();
    Serial.print((nbr_cell), DEC); Serial.println(" cells");
    Serial.print("soc = ");Serial.print((soc), DEC);Serial.println("%");
    Serial.print("Voltage = ");Serial.print((volt / 10), DEC);Serial.println(" V");
    Serial.print("Amperes = ");Serial.print((amp / 10), DEC);Serial.println(" A");
    Serial.print("Puissance = ");Serial.print((puiss), DEC);Serial.println(" Wh");
    Serial.print("Cell_av = ");Serial.print((cell_av), DEC);Serial.println(" mV");
    Serial.print("Cell_min = ");Serial.print((cell_min), DEC);Serial.println(" mV");
    Serial.print("Cell_max = ");Serial.print((cell_max), DEC);Serial.println(" mV");
    Serial.print("T_MOSFET = ");Serial.print((t_fet), DEC);Serial.println(" °C");
    Serial.print("T_BALANCE = ");Serial.print((t_bal), DEC);Serial.println(" °C");
    Serial.print("T_1 = ");Serial.print((t_1), DEC);Serial.println(" °C");
    Serial.print("T_2 = ");Serial.print((t_2), DEC);Serial.println(" °C");
    Serial.print("T_3 = ");Serial.print((t_3), DEC);Serial.println(" °C");
    Serial.print("T_4 = ");Serial.print((t_4), DEC);Serial.println(" °C");
    Serial.print("Cell_1 = ");Serial.print((cell_1), DEC);Serial.println(" mV");
    Serial.print("Cell_2 = ");Serial.print((cell_2), DEC);Serial.println(" mV");
    Serial.print("Cell_3 = ");Serial.print((cell_3), DEC);Serial.println(" mV");
    Serial.print("Cell_4 = ");Serial.print((cell_4), DEC);Serial.println(" mV");
    Serial.print("Cell_5 = ");Serial.print((cell_5), DEC);Serial.println(" mV");
    Serial.print("Cell_6 = ");Serial.print((cell_6), DEC);Serial.println(" mV");
    Serial.print("Cell_7 = ");Serial.print((cell_7), DEC);Serial.println(" mV");
    Serial.print("Cell_8 = ");Serial.print((cell_8), DEC);Serial.println(" mV");
    Serial.print("Cell_9 = ");Serial.print((cell_9), DEC);Serial.println(" mV");
    Serial.print("Cell_10 = ");Serial.print((cell_10), DEC);Serial.println(" mV");
    Serial.print("Cell_11 = ");Serial.print((cell_11), DEC);Serial.println(" mV");
    Serial.print("Cell_12 = ");Serial.print((cell_12), DEC);Serial.println(" mV");
    Serial.print("Cell_13 = ");Serial.print((cell_13), DEC);Serial.println(" mV");
    Serial.print("Cell_14 = ");Serial.print((cell_14), DEC);Serial.println(" mV");
    Serial.print("Cell_15 = ");Serial.print((cell_15), DEC);Serial.println(" mV");
    Serial.print("Cell_16 = ");Serial.print((cell_16), DEC);Serial.println(" mV");
    Serial.print("Cell_17 = ");Serial.print((cell_17), DEC);Serial.println(" mV");
    Serial.print("Cell_18 = ");Serial.print((cell_18), DEC);Serial.println(" mV");
    Serial.print("Cell_19 = ");Serial.print((cell_19), DEC);Serial.println(" mV");
    Serial.print("Cell_20 = ");Serial.print((cell_20), DEC);Serial.println(" mV");
    Serial.print("Cell_21 = ");Serial.print((cell_21), DEC);Serial.println(" mV");
    Serial.print("Cell_22 = ");Serial.print((cell_22), DEC);Serial.println(" mV");
    Serial.print("Cell_23 = ");Serial.print((cell_23), DEC);Serial.println(" mV");
    Serial.print("Cell_24 = ");Serial.print((cell_24), DEC);Serial.println(" mV");
    Serial.print("Ah_install = ");Serial.print((Ah_install / 1000000), DEC);Serial.println(" Ah");
    Serial.print("Ah_rest = ");Serial.print((Ah_rest / 1000000), DEC);Serial.println(" Ah");
    Serial.print("Ah_Total = ");Serial.print((Ah_Total / 1000), DEC);Serial.println(" Ah");
    Serial.print("trame ok/nok: ");Serial.print(trame_ok);Serial.print(" // ");Serial.print(trame_nok);Serial.print(" Ms: ");Serial.println((millis()) - (milli_start));
    Serial.println("_____________________________________________");
}

void setup()
{
    Serial.begin(115200);

    // UART 2
    _bms.begin(BMS_BAUD_RATE, SERIAL_8N1, BMS_RX_PIN, BMS_TX_PIN); // (19200,SERIAL_8N1,RXD2,TXD2)
    _bms.setTimeout(BMS_TIMEOUT); // How long to wait for received data

    pinMode(LED_BUILTIN, OUTPUT);

    byte errorCode;
    errorCode = VeCan.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);

    while (errorCode != CAN_OK)
    {
        Serial.print("CAN configuration error: ");
        Serial.println(errorCode, HEX);
        delay(500);
        errorCode = VeCan.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
    }

    if (errorCode == 0) {

        // One shot transmission
        VeCan.enOneShotTX();

        // Since we do not set NORMAL mode, we are in loopback mode by default.
        VeCan.setMode(MCP_NORMAL);

        pinMode(MCP2515_INT, INPUT_PULLUP); // Configuring pin for /INT input

        Serial.println("MCP2515 Initialized Successfully!");

        delay(500);
    }

    

    delay(100);

    return;
}

void loop() {
    static unsigned long lastBmsQuery = 0;

    if (checkTimer(&lastBmsQuery, Rate_ms))
    {
        read_bms();
    }


}




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