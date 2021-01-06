#include <Thread.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include "DHT.h"
#include <Adafruit_INA219.h>

#define DHTPIN 13 // DHT Sensor
#define DHTTYPE DHT11 // DHT 11

// LDR analog input pins
#define Light1PIN A0
#define Light2PIN A1
#define Light3PIN A2
#define Light4PIN A3

// Motor Pins
#define vMotorEnable 6
#define hMotorEnable 12
#define motorStep 4
#define motorDir 5

#define LED1 9 // Red (Setup and Error Indication)
#define LED2 8 // Green (Main loop)
#define LED3 7 // Blue (GSM Communication)

// Variables
int light1 = 0;
int light2 = 0;
int light3 = 0;
int light4 = 0;
double temp = 0;
double hum = 0;
double current_mA = 0;
double temp_sum = 0;
double hum_sum = 0;
double current_mA_sum = 0;
int read_count = 0;
double power_mW = 0;
double angleH = 0;
double angleV = 0;
String imei;
bool flag = false;
double lon = -1, lati;

SoftwareSerial myGsm(2, 3); // RX: 2, TX:3
SoftwareSerial gpsSerial(10, 11); // RX: 10, TX:11

unsigned char ctrl_z = 26;
// Instantiating a struct of dht , ina219
DHT dht(DHTPIN, DHTTYPE);
Adafruit_INA219 ina219;

void setup() {
  Serial.begin(9600);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  pinMode(vMotorEnable, OUTPUT);
  pinMode(hMotorEnable, OUTPUT);
  pinMode(motorStep, OUTPUT);
  pinMode(motorDir, OUTPUT);

  digitalWrite(vMotorEnable, HIGH); // active low
  digitalWrite(hMotorEnable, HIGH);

  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  delay(1000);

  dht.begin(); // initialize dht
  uint32_t currentFrequency;

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) {
      //Diagnostic LEDs ( LED1 Flashing 1 second --> INA219 Chip not found
      delay(1000);
      digitalWrite(LED1, HIGH);
      delay(1000);
      digitalWrite(LED1, LOW);
    }
  }

  //serial begin (with gps and gsm)
  gpsSerial.begin(9600);
  myGsm.begin(9600);

  while (!myGsm); // stall until gsm connects
  delay(1000);
  Serial.println("Start");
  getGps();

  digitalWrite(LED1, LOW); // LED1 2 pulses means GPS OK
  delay(500);
  digitalWrite(LED1, HIGH);
  delay(500);
  digitalWrite(LED1, LOW);
  delay(500);
  digitalWrite(LED1, HIGH);
  setupGsm();

  getImei();

  digitalWrite(LED1, LOW); // LED2 turning off means GSM OK

}

void moveVMotor(int s) {
  if (s == 0)
    digitalWrite(vMotorEnable, HIGH);
  else {
    digitalWrite(vMotorEnable, LOW);
    digitalWrite(motorDir, s == 1 ? HIGH : LOW);
    for (int i = 0; i < 2000; i++) {
      digitalWrite(motorStep, HIGH);
      delayMicroseconds(2000);
      digitalWrite(motorStep, LOW);
      delayMicroseconds(2000);
    }
    digitalWrite(vMotorEnable, HIGH);
  }
}

void moveHMotor(int s) {
  if (s == 0)
    digitalWrite(hMotorEnable, HIGH);
  else {
    digitalWrite(hMotorEnable, LOW);
    digitalWrite(motorDir, s == 1 ? HIGH : LOW);
    for (int i = 0; i < 2000; i++) {
      digitalWrite(motorStep, HIGH);
      delayMicroseconds(2000);
      digitalWrite(motorStep, LOW);
      delayMicroseconds(2000);
    }
    digitalWrite(hMotorEnable, HIGH);
  }
}

void ShowSerialData() {
  delay(500);
  while (myGsm.available()) {

    Serial.write(myGsm.read());
    //  delay(1);
  }
  delay(1500);
}
void sendData() { // function that sends data to server using gsm
  digitalWrite(LED3, HIGH); //  LED4 turning on means we are sending data
  myGsm.listen();
  delay(2000);
  String sendtoserver;

  //prepare data 
  sendtoserver = "{\"angleH\":";
  sendtoserver += angleH;
  sendtoserver += ",\"angleV\":";
  sendtoserver += angleV;
  sendtoserver += ",\"hum\":";
  sendtoserver += hum;
  sendtoserver += ",\"lat\":";
  sendtoserver += lati;
  sendtoserver += ",\"lon\":";
  sendtoserver += lon;
  sendtoserver += ",\"output\":";
  sendtoserver += (power_mW / 1000); // power in Watts
  sendtoserver += ",\"temp\":";
  sendtoserver += temp;
  sendtoserver += "}";

  Serial.println(sendtoserver);

  //commands to get imei, obtain ip address, open connection and send post request
  myGsm.println("AT+CIPSHUT\r");
  delay(500);
  ShowSerialData();
  myGsm.println("AT+CIPMUX=0\r");
  delay(500);
  ShowSerialData();
  myGsm.println("AT+CGATT=1\r");
  delay(1000);
  ShowSerialData();

  myGsm.println("AT+CSTT=\"net.orange.jo\",\"net\",\"net\"\r");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CIICR\r");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CIFSR\r");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CIPSTART=\"TCP\",\"52.91.175.107\", 8082\r");
  delay(500);
  ShowSerialData();

  delay(500);
  myGsm.println("AT+CIPSEND\r");
  delay(500);
  ShowSerialData();

  myGsm.println("POST /arduino/" + imei + " HTTP/1.1");
  delay(500);
  ShowSerialData();

  myGsm.println("Host: 52.91.175.107");
  delay(500);
  ShowSerialData();

  myGsm.println("Content-Type: application/json");
  delay(500);
  ShowSerialData();

  myGsm.println("Content-Length: " + String(sendtoserver.length()) + "\r\n");
  delay(500);
  ShowSerialData();

  myGsm.println(sendtoserver);
  delay(500);
  ShowSerialData();

  myGsm.write(ctrl_z);
  ShowSerialData();
  // Blinking Green means attempting to send data
  digitalWrite(LED3, LOW);
  delay(1000);
  digitalWrite(LED3, HIGH);
  delay(1000);
  digitalWrite(LED3, LOW);
  delay(1000);
  digitalWrite(LED3, HIGH);
  delay(1000);
  digitalWrite(LED3, LOW);
  delay(1000);
  digitalWrite(LED3, HIGH);
  delay(1000);
  digitalWrite(LED3, LOW);
  delay(1000);
  digitalWrite(LED3, HIGH);
  delay(1000);
  digitalWrite(LED3, LOW);
  delay(1000);
  digitalWrite(LED3, HIGH);
  delay(1000);
  ShowSerialData();

  myGsm.println("AT+CIPCLOSE");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CIPSHUT");
  delay(500);
  ShowSerialData();
  digitalWrite(LED3, LOW);

}

void getImei() { // function to read device IMEI (used in endpoint URL)
  imei = "";
  do {
    myGsm.listen();
    delay(2000);
    myGsm.println("AT+GSN\r\n");
    delay(1500);
    while (myGsm.available()) {
      imei += myGsm.readStringUntil('\n');
    }
    imei.replace("AT+GSN", "");
    imei.replace("OK", "");
    imei.replace("\n", "");
    imei.replace("\r", "");
    Serial.println("imei=" + imei);
    ShowSerialData();
  } while (imei == "");
}

void getGps() { // function to get GPS coordinates
  gpsSerial.listen();
  Serial.println("getGPS");
  TinyGPSPlus gps;

  delay(2000);
  do {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
      if (gps.location.isUpdated()) {
        Serial.println("getGPS3");
        Serial.print("Latitude= ");
        Serial.print(gps.location.lat(), 6);
        lati = gps.location.lat();
        Serial.print(" Longitude= ");
        Serial.println(gps.location.lng(), 6);
        lon = gps.location.lng();
        flag = true;
      }
    }
  } while (!flag);
  gpsSerial.flush();
  myGsm.flush();
  delay(1000);
}

void setupGsm() {

  myGsm.listen();
  Serial.println("Setup GSM");
  delay(2000);

  myGsm.println("AT");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CSQ");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CCID");
  delay(500);
  ShowSerialData();

  myGsm.println("AT+CREG?");
  delay(500);
  ShowSerialData();

}

int counter = 0;
void loop() {

  // each loop is measured as ~10 seconds
  if (counter == 30) { // ~5 minute interval
    //if (counter == 180){ // ~30 minute interval
    // average values over the past interval
    current_mA = current_mA_sum / read_count;
    hum = hum_sum / read_count;
    temp = temp_sum / read_count;
    power_mW = (current_mA * current_mA * 15) / 1000; // PV voltage is ~15
    counter = 2; // start from 2 to account for the 20 second delay from sendData function
    read_count = 0;
    sendData();
  }

  digitalWrite(LED2, HIGH); //  LED3 on --> Main loop

  delay(2000);
  read_count++;
  hum = dht.readHumidity();
  hum_sum += hum;
  temp = dht.readTemperature();
  temp_sum += temp;
  light1 = analogRead(Light1PIN);
  light2 = analogRead(Light2PIN);
  light3 = analogRead(Light3PIN);
  light4 = analogRead(Light4PIN);

  // Check if any reads failed
  if (isnan(hum) || isnan(temp)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    digitalWrite(LED1, HIGH); // LED1 turning on means problem with DHT sensor
    return;
  }

  if (isnan(light1) || isnan(light2) || isnan(light3) || isnan(light4)) {
    Serial.println(F("Failed to read from LDR sensor!"));
    digitalWrite(LED2, HIGH); // LED1 turning on means problem with light sensors
    return;
  }
  delay(2000);
  current_mA = ina219.getCurrent_mA() * 10;
  if ((current_mA) < 0) current_mA = 0;
  current_mA_sum += current_mA;

  // Motor movement

  if (light1 > (light2 + 150)) moveVMotor(1);
  else if (light2 > (light1 + 150)) moveVMotor(2);

  if (light3 > (light4 + 150)) moveHMotor(1);
  else if (light4 > (light3 + 150)) moveHMotor(2);

  delay(2000);
  Serial.print(F("Humidity: "));
  Serial.print(hum);
  Serial.print(F("%  Temperature: "));
  Serial.print(temp);
  Serial.print(F("°C \n"));
  Serial.flush();
  delay(2000);
  Serial.print(F("Light 1: "));
  Serial.println(light1);
  Serial.print(F("Light 2: "));
  Serial.println(light2);
  Serial.print(F("Light 3: "));
  Serial.println(light3);
  Serial.print(F("Light 4: "));
  Serial.println(light4);
  Serial.flush();
  delay(2000);
  Serial.print("Current:       ");
  Serial.print(current_mA);
  Serial.println(" mA");
  Serial.print("Power:         ");
  Serial.print(power_mW);
  Serial.println(" mW");
  Serial.println("");
  Serial.flush();

  digitalWrite(LED2, LOW); //  LED3 on --> Main loop

  counter++;

}
