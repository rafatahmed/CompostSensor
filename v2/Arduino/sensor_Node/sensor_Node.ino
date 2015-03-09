#include <RFM69.h>
#include <SPI.h>
#include <Ports.h>
#include <Wire.h>
#include <SHT2x.h>

#define NODEID      11
#define NETWORKID   100
#define GATEWAYID   1
#define FREQUENCY   RF69_433MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "thisIsEncryptKey" //has to be same 16 characters/bytes on all nodes, not more not less!
#define LED         9
#define SERIAL_BAUD 9600
#define ACK_TIME    50  // # of ms to wait for an ack
#define NB_ATTEMPTS_ACK 5 //number of attempts to try before giving up

int TRANSMITPERIOD = 20000; //transmit a packet to gateway so often (in ms) (one minute)
int TRANSMITPERIOD_MINUTES = 1;
float batteryVoltage;
byte sendSize=0;
boolean requestACK = true;
RFM69 radio;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

typedef struct {		
    int              nodeId; //store this nodeId
    unsigned long    uptime; //uptime in ms
    float temp;
    float humidity;
    float voltage;
} Payload;
Payload payload;

void setup() {
    Serial.begin(SERIAL_BAUD);
    Wire.begin();
    radio.initialize(FREQUENCY,NODEID,NETWORKID);
    radio.setHighPower(); //uncomment only for RFM69HW!
    //radio.setPowerLevel(20); //Set power level for Radio.
    radio.encrypt(KEY);
    radio.sleep();
    char buff[50];
    sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
}

long lastPeriod = -1;

void loop() {
    int nAttempt = 0; //number of sends attempted
    bool flag_ACK_received = false; //is the acknowledgement recieved?
    float t = SHT2x.GetTemperature();
    float rh =  SHT2x.GetHumidity();
    float ah = massOfWater(t,rh);
    payload.nodeId = NODEID;
    payload.uptime = millis()/1000;
    payload.temp = t;
    payload.humidity = ah;
    //payload.abshumid = ah;
    payload.voltage = checkBatteryVoltage();
    /*
    payload.humidity = 0;
    payload.temp = 0;
    payload.voltage = 0;
    */
    /*
    Serial.println();
    Serial.print("Humidity(%RH): ");
    Serial.println(payload.humidity);
    Serial.print("Temperature(C): ");
    Serial.println(payload.temp);
    Serial.print("Voltage: ");
    Serial.println(payload.voltage);
    Serial.print("Sending struct (");
    Serial.print(sizeof(payload));
    Serial.print(" bytes) ... ");
*/
        while(nAttempt < NB_ATTEMPTS_ACK && !flag_ACK_received) { //resend package if it doesn't go through
        if (radio.sendWithRetry(GATEWAYID, (const void*)(&payload), sizeof(payload))){
            Serial.print(" ok!");
            flag_ACK_received = true;
            Blink(LED, 100);
        } else {
            //figure out a way to resend at a higher power
            //radio.setPowerLevel(31);
            //this isn't implemented!
            Serial.print("- Failed, ");
            Serial.println("sending again...");
            radio.sendWithRetry(GATEWAYID, (const void*)(&payload), sizeof(payload));
            Blink(LED, 100);
            delay(100);
            Blink(LED, 100);
            delay(100);
            Blink(LED, 100);
            delay(100);
            nAttempt++;
            Sleepy::loseSomeTime(5000);
        }
    }
    Serial.println();
    Blink(LED,3);
    delay(100);
    radio.sleep();
    for(int i = 0; i < TRANSMITPERIOD_MINUTES; i++) {
        Sleepy::loseSomeTime(TRANSMITPERIOD);
    }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,LOW);
  delay(DELAY_MS);
  digitalWrite(PIN,HIGH);
}

float checkBatteryVoltage() {
    int readVal = 0;
    for (int i = 0; i < 10; i++) {
        readVal += analogRead(0);
        delay(10);
    }
    readVal = readVal/10;
    batteryVoltage = (3.3 * readVal/1024.0); //this should be readVal/1023? 10 bit, 1024 values, 0-1023
    return batteryVoltage;
}

//these two functions are from here: https://github.com/jgeisler0303/VentingController
float massOfWater(float T, float RH) {
  const float P= 1013.25; // pressure hPa (one atmosphere)
  const float M_H2O= 18.01528; // molar mass of water g/mol
  const float R= 8.3144621; // Universal gas constant J/(mol*K)
  float P_H2O= RH/100*vaporPressure(T); // partial pressure of water hPa
  float x_H2O= P_H2O/P; // mass concentration of water -
  float n_air= (P*100)/(R*(T+273.15)); // air density mol/m^3
  float c_H2O= x_H2O*n_air*M_H2O; // mass concentration of water g/m^3

  // c_H2O_= 216.7*(RH/100*6.112*exp(17.62*T/(243.12+T))/(273.15+T));
  // float c_H2O_= 100/R*M_H2O*(RH/100*6.112*exp(17.62*T/(243.12+T))/(273.15+T));
  return(c_H2O);
}
float vaporPressure(float T) {
  const float a0= 6.107799961;
  const float a1= 4.436518521e-1;
  const float a2= 1.428945805e-2;
  const float a3= 2.650648471e-4;
  const float a4= 3.031240396e-6;
  const float a5= 2.034080948e-8;
  const float a6= 6.136820929e-11;
  return(a0+T*(a1+T*(a2+T*(a3+T*(a4+T*(a5+T*a6))))));
}

