#include <Arduino.h>
#include <ModbusMaster.h>
#include <WiFiNINA.h>
#include <SPI.h>
#include <SD.h>

#include <PubSubClient.h>

#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

#include <RTCZero.h>
#include <avr/dtostrf.h>
#include <iomanip>

/* Create an rtc object */
RTCZero rtc;

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 0;
const byte hours = 11;

/* Change these values to set the current initial date */
const byte day = 23;
const byte month = 3;
const byte year = 24;

char auth[] = "vK2z96dcRVNRZojcHeYw8YQSy_D55mqT";

char ssid[] = "DeterminedGuy";
char pass[] = "athalganteng";
const char* googleApiKey = "AIzaSyCm6P9KfhQK5PwbwYXka83_k3OXVuCiLxU";
const char blynk_server [] = "blynk.cloud"; 
const int blynk_port = 8080;  
const char* mqtt_server = "34.123.176.43";

const char* sensor_id = "sensor1";

// MicroSD
#define FILE_BASE_NAME "Data"
File my_file;
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
char file_name[] = FILE_BASE_NAME "00.csv";
#define pin_cs 4
int writeSDCardState = 0;

void setupSDcard(){
  // inisialisasi rekam data
  pinMode(pin_cs, OUTPUT);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(pin_cs)) {
    Serial.println("Card failed, or not present");
    while (1);
  }
  Serial.println("card initialized.");

  while (SD.exists(file_name)) {
    if (file_name[BASE_NAME_SIZE + 1] != '9') {
      file_name[BASE_NAME_SIZE + 1]++;
    } else if (file_name[BASE_NAME_SIZE] != '9') {
      file_name[BASE_NAME_SIZE + 1] = '0';
      file_name[BASE_NAME_SIZE]++;
    } else {
      Serial.println(F("Can't create file name"));
      return;
    }
  }
}

// Blynk
void WifiConnect(){
  Serial.println(F("Connecting to Wi-Fi"));
  
  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, pass); 
  }
  
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");
  }
  
  if(WiFi.status() == WL_CONNECTED){
    Serial.println(F("Wi-Fi CONNECTED"));
  }
}

void setupRTC(){
  rtc.begin(); // initialize RTC

  //Set the time
  rtc.setHours(hours);
  rtc.setMinutes(minutes);
  rtc.setSeconds(seconds);

  // Set the date
  rtc.setDay(day);
  rtc.setMonth(month);
  rtc.setYear(year);
}

void getTimeStamp(){
    // Print date...
  Serial.print(rtc.getDay());
  Serial.print("/");
  Serial.print(rtc.getMonth());
  Serial.print("/");
  Serial.print(rtc.getYear());
  Serial.print("\t");

  // ...and time
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.print(rtc.getSeconds());

  Serial.println();

  delay(1000);
}

// Modbus
ModbusMaster node1, node2;

#define MAX485_DE      3  //control pin first MAX485  
#define MAX485_RE_NEG  2  //control pin first MAX485 

int16_t rika1[2], rika2[2];
float temperature1, humidity1, temperature2, humidity2;
int readModbusRS485State = 0;

void preTransmission(){
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission(){
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void setupModbusRS485(){
  
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);

  // Init in receive mode
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);

  // delay(10);

  //baudrate for master arduino - slave sensor
  Serial1.begin(9600);

  // Sensor Modbus slave ID is 1
  node1.begin(1, Serial1);
  node2.begin(2, Serial1);

  // Callbacks allow us to configure the RS485 transceiver correctly
  node1.preTransmission(preTransmission);
  node1.postTransmission(postTransmission);

  node2.preTransmission(preTransmission);
  node2.postTransmission(postTransmission);
}

// Battery 
int batteryPin = A1;
int batteryMeasureState = 0;
float batteryPercentage; 

void batteryMeasure(){
  long sum = 0;                   
  float voltage = 0.0;            
  float output = 0.0;             
  const float batteryMaxVoltage = 4.1;
  const float batteryMinVoltage = 3.3;

  if (batteryMeasureState == 0){
    for (int i = 0; i < 500; i++){
      sum += analogRead(batteryPin);
      delayMicroseconds(1000);
    }
    
    voltage = sum / (float) 500;
    voltage = (voltage * 4.39) / 1023.0; 
    voltage = roundf(voltage * 100) / 100;
    output = ((voltage - batteryMinVoltage) / (batteryMaxVoltage - batteryMinVoltage)) * 100;

    if (output <= 0.00){
      batteryPercentage = 0.00;
    } else if (output > 100.00){
      batteryPercentage = 100.00;
    } else {
      batteryPercentage = output;
    }
  }
  Serial.print("Battery Percentage: ");
  Serial.println(output);
  batteryMeasureState = 1;
}

int32_t keller[4];
char pressure1[5], pressure2[5], tob1[5], tob2[5];
float final_pressure, final_temperature;

void readModbusRS485(){
  uint8_t result, result2;
  
  if (readModbusRS485State == 0){
    result = node1.readHoldingRegisters(0x00, 0x02);
    Serial.print("Error code: ");
    Serial.println(result);

    if (result == node1.ku8MBSuccess){
      for(int j = 0; j<2; j++){
        rika1[j] = node1.getResponseBuffer(j);
      }
    }

    Serial.print("First Array Value: "); Serial.println(rika1[0], HEX);
    Serial.print("Seccond Array Value: "); Serial.println(rika1[1], HEX);  

    Serial.print("Temperature value: "); 
    temperature1 = float(rika1[0])/10;
    Serial.println(temperature1);

    Serial.print("Humidity value: "); 
    humidity1 = float(rika1[1])/10;
    Serial.println(humidity1); 

    delay(5000);
    Serial.println("====================================");

    result2 = node2.readHoldingRegisters(0x0100, 0x04);
    Serial.print("Error code: ");
    Serial.println(result2);

    if (result2 == node2.ku8MBSuccess){
      for(int j = 0; j<4; j++){
        keller[j] = node2.getResponseBuffer(j);
        int intValue;
        memcpy(&intValue, &keller[j], sizeof(float));

        // Convert the integer to hexadecimal with 4 digits
        char hexString[5]; // Adjust the size based on your needs
        sprintf(hexString, "%04X", intValue); // %04X ensures 4 digits with leading zeros

        if (j == 0) {
        strncpy(pressure1, hexString, sizeof(pressure1) - 1);
        pressure1[sizeof(pressure1) - 1] = '\0';  // Ensure null termination
        } else if (j == 1) {
            strncpy(pressure2, hexString, sizeof(pressure2) - 1);
            pressure2[sizeof(pressure2) - 1] = '\0';  // Ensure null termination
        } else if (j == 2) {
            strncpy(tob1, hexString, sizeof(tob1) - 1);
            tob1[sizeof(tob1) - 1] = '\0';  // Ensure null termination
        } else {
            strncpy(tob2, hexString, sizeof(tob2) - 1);
            tob2[sizeof(tob2) - 1] = '\0';  // Ensure null termination
        }
          }
    }

    char pressure[11];  // Adjust the size based on your needs
    strcpy(pressure, pressure1);
    strcat(pressure, pressure2);

    char temperature[11];  // Adjust the size based on your needs
    strcpy(temperature, tob1);
    strcat(temperature, tob2);

    Serial.println(pressure);
    Serial.println(temperature);

    Serial.print("pressure value: "); 
    uint32_t num;
    sscanf(pressure, "%x", &num);  // assuming you checked input
    final_pressure = *((float*)&num);
    Serial.println(final_pressure);

    Serial.print("temperature value: "); 
    uint32_t num1;
    sscanf(temperature, "%x", &num1);  // assuming you checked input
    final_temperature = *((float*)&num1);
    Serial.println(final_temperature);    

    Serial.println("====================================");
    delay(5000);
  }
  readModbusRS485State = 1;
}

// Moisture DFROBOT
int powerMoist = 1, analogMoist1 = A2, analogMoist2 = A3, readMoistureDFRObotState = 0;
float moist1, moist2;
void readMoistureDFRObot(){
  digitalWrite(powerMoist, HIGH);
  delay(3000);
  if (readMoistureDFRObotState == 0){
    
    moist1 = analogRead(analogMoist1);
    Serial.print("Moisture DFRobot 1: ");
    Serial.println(moist1);
    delay(10);
    
    moist2 = analogRead(analogMoist2);
    Serial.print("Moisture DFRobot 2: ");
    Serial.println(moist2);
    delay(10);  
    digitalWrite(powerMoist, LOW);
  }
  readMoistureDFRObotState = 1;
}

void writeSDcard(){
  // Serial.println(writeSDCardState);
  my_file = SD.open(file_name, FILE_WRITE);
  if (my_file){
    my_file.print(temperature1);my_file.print(',');
    my_file.print(humidity1);my_file.print(',');
    my_file.print(temperature2);my_file.print(',');
    my_file.print(humidity2);my_file.print(',');
    my_file.println(); my_file.close();
    writeSDCardState = 1;
  }
  // Serial.println(writeSDCardState);
}


void resetState(){
  batteryMeasureState = 0;
  readModbusRS485State = 0;
  readMoistureDFRObotState = 0;
  writeSDCardState = 0;
}

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void clientCallback(char *topic, uint8_t *payload, unsigned int length)
{
    char buff[length + 1];
    for (int i = 0; i < length; i++)
    {
        buff[i] = (char)payload[i];
    }
    buff[length] = '\0';

    Serial.print("Message received:");
    Serial.println(buff);
    String message = buff;
    Serial.println(message);

}

void reconnectMQTTClient()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        if (client.connect("arduino-client"))
        {
            Serial.println("connected");     
            client.subscribe("sensor_mqtt");
        }
        else
        {
            Serial.print("Retying in 5 seconds - failed, rc=");
            Serial.println(client.state());            
            delay(5000);
        }
    }
}

void createMQTTClient()
{
    client.setServer(mqtt_server, 1883);
    client.setCallback(clientCallback);
    reconnectMQTTClient();
}

void sendSensorDataToMQTT( float temperature, float humidity, float pressure) {
  // Create a JSON document
  const size_t bufferSize = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument jsonDoc(bufferSize);

  char dateTime[20];

  sprintf(dateTime, "%04d-%02d-%02d %02d:%02d:%02d",
           rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(),
          rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

  Serial.println(dateTime);

  jsonDoc["timestamp"] = dateTime;
  jsonDoc["longitude"] = 106.8314781;
  jsonDoc["latitude"] = -6.3669607;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["pressure"] = pressure;
  jsonDoc["sensor_id"] = sensor_id;

  // Serialize JSON to a string
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Print JSON string to Serial Monitor for debugging
  Serial.println("Serialized JSON data:");
  Serial.println(jsonString);

  client.publish("sensor_mqtt", jsonString.c_str());
  Serial.println("Data sent to mqtt");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  WifiConnect();
  setupRTC();
  setupModbusRS485();
  //setupSDcard();
  createMQTTClient();
  pinMode(powerMoist, OUTPUT);
}

void loop() {
  //put your main code here, to run repeatedly:
  //digitalWrite(LED_BUILTIN, HIGH);
  // batteryMeasure();

  // Your loop code here
  delay(1000);  // Add delay if necessary

  // runNTP();
  getTimeStamp();
  readModbusRS485(); //uncomment
  // readMoistureDFRObot();
  // writeSDcard(); //uncomment
  // sendData(); //uncomment
  // sendSensorData(temperature1, humidity1, 0);
  sendSensorDataToMQTT(temperature1, humidity1, 0);
  // delay(5000);
  resetState(); //uncomment
  // digitalWrite(LED_BUILTIN, LOW);
  // Blynk.run();
  delay(1000);
}


// ------------------------ OLD CODE (not used)
// Send Data using HTTP (Alternative)

// const char* server = "192.168.103.206";
// const char serverName[] = "api-project-tx2erenz2a-uc.a.run.app";
// const int port = 8000; // Adjust the port if needed
// WiFiSSLClient client;

// void sendSensorData( float temperature, float humidity, float pressure) {
//   // Create a JSON document
//   const size_t bufferSize = JSON_OBJECT_SIZE(6);
//   DynamicJsonDocument jsonDoc(bufferSize);

//   char dateTime[20];

//   sprintf(dateTime, "%04d-%02d-%02d %02d:%02d:%02d",
//            rtc.getYear() + 2000, rtc.getMonth(), rtc.getDay(),
//           rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

//   Serial.println(dateTime);

//   jsonDoc["timestamp"] = dateTime;
//   jsonDoc["longitude"] = 106.8314781;
//   jsonDoc["latitude"] = -6.3669607;
//   jsonDoc["temperature"] = temperature;
//   jsonDoc["humidity"] = humidity;
//   jsonDoc["pressure"] = pressure;

//   // Serialize JSON to a string
//   String jsonString;
//   serializeJson(jsonDoc, jsonString);

//   // Print JSON string to Serial Monitor for debugging
//   Serial.println("Serialized JSON data:");
//   Serial.println(jsonString);

//   // String contentType = "application/json";

//   // // Send HTTP POST request with JSON data
//   // client.post("/insert_data", contentType, jsonString);

//   while (client.available()) {

//     char c = client.read();

//     Serial.write(c);
//   }

//   //connect to server and post data
//   if (client.connect(serverName, 443)) {
//     Serial.println(client);
//     Serial.println("connected to server");


//     client.println("POST /insert_data HTTP/1.1");
//     client.println("Host: " + String(serverName));
//     // client.println("User-Agent: ArduinoWiFi/1.1");
//     client.println("Content-Type: application/json");
//     client.println("Connection: close");
//     client.print("Content-Length: ");
//     client.println(jsonString.length());
//     client.println();
//     client.print(jsonString);
//     delay(10000);
//   }

//   // client.beginRequest();
//   // client.post("/insert_data"); // Adjust the path if needed
//   // client.sendHeader("Content-Type", "application/json");
//   // client.sendHeader("Content-Length", jsonString.length());
//   // client.sendHeader("Connection", "close");
//   // client.beginBody();
//   // client.print(jsonString);
//   // client.endRequest();

//   // // Wait for the server response
//   // int statusCode = client.responseStatusCode();
//   // String response = client.responseBody();
//   // // String newUrl = client.header("Location");

//   // Serial.print("HTTP Status Code: ");
//   // Serial.println(statusCode);
//   // Serial.print("Server Response: ");
//   // Serial.println(response);
// }

