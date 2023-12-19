#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <WiFi.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "_____"
#define WIFI_PASSWORD "_______"

// Insert Firebase project API Key
#define API_KEY "______"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "_______" 

#define NUMCHILDPATH 3
String parentPath = "/iot/sensor";
String childPath[NUMCHILDPATH] = {"/ldr","/motor","/rain"};
String childData[NUMCHILDPATH]; //data[0]->ldr || data[1]->motor || data[2]-> rain || data[3]->\0

//Define sensor
#define RAIN_SENSOR_PIN 35
#define LIGHT_SENSOR_PIN 34
// Motor A
int motor1Pin1 = 27; 
int motor1Pin2 = 26; 
int enable1Pin = 14; 

// Setting PWM properties
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;

//Define Firebase Data object
FirebaseData fbdo;
FirebaseJsonData result;
FirebaseAuth auth;
FirebaseConfig config;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7*60*60;
const int   daylightOffset_sec = 7*60*60;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup(){
  Serial.begin(115200);
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  // configure LED PWM functionalitites
  ledcSetup(pwmChannel, freq, resolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(enable1Pin, pwmChannel);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop(){
  int uploadStatus = 0, ldrValue = analogRead(LIGHT_SENSOR_PIN), rainValue = analogRead(RAIN_SENSOR_PIN);
  FirebaseJson dataUpload;
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.getJSON(&fbdo, parentPath)) {
      if (fbdo.dataTypeEnum() == firebase_rtdb_data_type_json) {
        FirebaseJson *json = fbdo.to<FirebaseJson *>();
        Serial.println(json->raw());
        for(int i = 0; i < NUMCHILDPATH; i++){
        json->get(result, childPath[i]);
        if (result.success){
        childData[i] = result.to<String>();
        }  
      } 
    }
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  Serial.print("LDR: ");
  Serial.println(childData[0]);
  Serial.print("Motor: ");
  Serial.println(childData[1]);
  Serial.print("Rain: ");
  Serial.println(childData[2]);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Rain Drop: ");
  Serial.print(rainValue);
  Serial.print("  ||  ");
  Serial.print("Analog Value = ");
  Serial.println(ldrValue); 
  //Dark(-)
  if(ldrValue <= 2000 && !(childData[0].toInt() == 0)){
    childData[0]= "0";
    childData[1]= "2";
    dataUpload.set(childPath[0],childData[0]);
    dataUpload.set(childPath[1],childData[1]);
    uploadStatus = 1;
  }else{
    dataUpload.set(childPath[0],childData[0]);
  }
  //Light(+)
  if (ldrValue > 2000 && !(childData[0].toInt() == 1)){
    childData[0]= "1";
    childData[1]= "1";
    dataUpload.set(childPath[1],childData[1]);
    dataUpload.set(childPath[0],childData[0]);
    uploadStatus = 1;
  }else{
    dataUpload.set(childPath[0],childData[0]);
  }
  //Light(+)
  if(rainValue <= 2000 && !(childData[2].toInt() == 1)){
    childData[2]= "1";
    childData[1]= "1";
    dataUpload.set(childPath[1],childData[1]);
    dataUpload.set(childPath[2],childData[2]);
    uploadStatus = 1;
  }else{
    dataUpload.set(childPath[2],childData[2]);
  }
  //Rain(-)
  if (rainValue > 2000 && !(childData[2].toInt() == 0)){
    childData[2]= "0";
    childData[1]= "2";
    dataUpload.set(childPath[1],childData[1]);
    dataUpload.set(childPath[2],childData[2]);
    uploadStatus = 1;
  }else{
    dataUpload.set(childPath[2],childData[2]);
  }
  //Motor
  if(childData[1].toInt() == 1){
    Serial.println("Moving Forward");
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH); 
    delay(2000);
    childData[1]= "0";
    dataUpload.set(childPath[1],childData[1]);
    uploadStatus = 1;
  }else if(childData[1].toInt() == 2){
    Serial.println("Moving Backwards");
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW); 
    delay(2000);
    childData[1]= "0";
    dataUpload.set(childPath[1],childData[1]);
    uploadStatus = 1;
  }else{
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    dataUpload.set(childPath[1],childData[1]);
  }
  //Upload Firebase
  if(uploadStatus == 1){
    Firebase.RTDB.setJSON(&fbdo,parentPath,&dataUpload);
    uploadStatus == 0;
  }
  delay(1000);
}
