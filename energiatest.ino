#include <WiFi.h>
#include <Wire.h>
#include "Adafruit_TMP006.h"

/**************** Configuration ****************/

// Seconds between readings/postings
#define POSTING_INTERVAL_MSEC  20000

// Milliseconds to wait for server-reply. Set to 0 when not debugging.
#define WAIT_FOR_REPLY_MSEC    400

// Voltage divider factor
#define ADC_VOLT_DIV_FACTOR    5

// Misc. Probably don't touch these
#define MAX_VALUE_STRING_LEN   20
#define MAX_MESSAGE_BODY_SIZE  600
#define HTTP_PORT              80
#define CC3200_ADC_REF_MV      1400 // From TI CC3200 Launchpad SDK ADC example code
#define CC3200_ADC_MAX_READING 4095

// Network info
char ssid[] = "Nyskapingsparken";
char password[] = "";

// Friskby server, name or IP
const char server[] = "friskby.herokuapp.com";
//IPAddress server(192,168,1,42);

// Sensor struct - do not change
union Reading {
  int asInt;
  float asFloat;
};

struct sensor {
  char* sensorId;
  char* apiKey;
  Reading reading;
  void (*initSensor)();
  void (*readSensor)();  // Note! readSensor() must populate valueStr[]
  char valueStr[MAX_VALUE_STRING_LEN]; // Value as a string (will be used in msg payload)
};

// Sensors - Add new sensors below and update NUM_SENSORS
// Examples
// sensor sensorName = {"SensorIDfromFriskBy", "APIKeyFromFriskBy", 0.0f, nameOfFunctionDoingInit, nameOfFunctionDoingReading, {0}};
sensor dieTemp = {"ARUtest", "d6b065e8-42a7-4bd8-90bc-df939d91de99", 0.0f, initDieTempSensor, readDieTempSensor, {0}};
sensor dust = {"ARUdustTest", "70aaa8c4-db8f-45d7-a713-e6add4d0d52d", 0.0f, initDustSensor, readDustSensor, {0}};

#define NUM_SENSORS  2

// Add sensor-pointer to listOfSensors
sensor* listOfSensors[NUM_SENSORS] = {&dieTemp, &dust};

// Sensor-specific config
#define TEMP_I2C_ADDRESS           0x41
#define DUST_SENSOR_ADC_PIN        6   // ADC Channel 2, Pin 59 on board (ADC Ch.1 (pin 58) gave unstable readings)
#define DUST_SENSOR_DIG_PIN        5   // Pin 61 on board
#define DUST_SENSOR_SAMPLING_TIME  280 // From http://www.dfrobot.com/wiki/index.php/Sharp_GP2Y1010AU
#define DUST_SENSOR_DELTA_TIME     40

/**************** End of configuration ****************/

// Data structures
unsigned long lastConnectionTime = 0;
WiFiClient client;
char timestamp[] = "2015-11-10T22:32:00Z";
Adafruit_TMP006 tmp006(TEMP_I2C_ADDRESS);

void setup() {
  // Initialize serial
  Serial.begin(115200);

  // Connect to WPA/WPA2 network.
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nConnected");
  Serial.println("Waiting for IP address");

  // Wait for IP
  while (WiFi.localIP() == INADDR_NONE) {
    Serial.print(".");
    delay(1000);
  }

  printWifiStatus();

  // Connected. Init sensors.
  for(int i = 0; i < NUM_SENSORS; i++) {
    listOfSensors[i]->initSensor();
  }
}

void loop() {
  // Read sensors
  for(int i = 0; i < NUM_SENSORS; i++) {
    listOfSensors[i]->readSensor();
  }

  // Send data to server
  postToServer();

  // Note! There is additional sleeping in sensor readings and server reply
  sleep(POSTING_INTERVAL_MSEC);
}

void initDieTempSensor() {
  // Init communication with temp sensor
  if (!tmp006.begin()) {
    Serial.println("Temp sensor not found");
    while(1);
  }
}

void readDieTempSensor() {
  tmp006.wake();
  dieTemp.reading.asFloat = tmp006.readDieTempC();

  // Sleep caused bad readings. But we are not worried about power consumption atm
  // TODO Dig into datasheets, probably need some delay after wake()
  // tmp006.sleep();

  // Create string of the float value (sprintf with float not supported(and will cause undefined behavior))
  memset(dieTemp.valueStr, 0, MAX_VALUE_STRING_LEN);
  int intPart = (int)dieTemp.reading.asFloat;
  int fractPart = (dieTemp.reading.asFloat * 100) - (intPart * 100);
  sprintf(dieTemp.valueStr,"%d.%02d", intPart, fractPart);

  Serial.print("DieTemp-sensor reading: ");
  Serial.println(dieTemp.valueStr);
}

void initDustSensor() {
  pinMode(DUST_SENSOR_DIG_PIN, OUTPUT);
}

void readDustSensor() {
  // Turn on LED
  digitalWrite(DUST_SENSOR_DIG_PIN, LOW);
  sleep(DUST_SENSOR_SAMPLING_TIME);

  // Take reading
  // Raw ADC value
  dust.reading.asInt = analogRead(DUST_SENSOR_ADC_PIN) * ADC_VOLT_DIV_FACTOR;
  // Millivolt
  //dust.reading.asInt = ((analogRead(DUST_SENSOR_ADC_PIN)  * CC3200_ADC_REF_MV * ADC_VOLT_DIV_FACTOR) / CC3200_ADC_MAX_READING);

  sleep(DUST_SENSOR_DELTA_TIME);

  // Turn off LED
  digitalWrite(DUST_SENSOR_DIG_PIN, HIGH);

  // Create string
  memset(dust.valueStr, 0, MAX_VALUE_STRING_LEN);
  sprintf(dust.valueStr, "%d", dust.reading.asInt);

  Serial.print("Dust-sensor reading: ");
  Serial.print(dust.valueStr);
  Serial.println("");
}

void postToServer() {
  // If connection successful
  if (client.connect(server, HTTP_PORT)) {

    // Iterate sensors and add to message body
    for(int i = 0; i < NUM_SENSORS; i++) {
      char msgBody[MAX_MESSAGE_BODY_SIZE] = {0};
      sprintf(
        msgBody,
        "{\"sensorid\":\"%s\",\"timestamp\":\"%s\",\"value\":%s,\"key\":\"%s\"}",
        listOfSensors[i]->sensorId, timestamp, listOfSensors[i]->valueStr, listOfSensors[i]->apiKey
      );

      Serial.print("Sending Message for sensor ");
      Serial.print(listOfSensors[i]->sensorId);
      Serial.print("\nBody: ");
      Serial.println(msgBody);
      Serial.print("Message length: ");
      Serial.println(strlen(msgBody));

      // Send request
      client.println("POST /sensor/api/reading/ HTTP/1.1");
      client.print("Host: ");
      client.println(server);
      client.println("User-Agent: Energia/1.1");
      client.print("Content-length:");
      client.println(strlen(msgBody));
      client.println("Content-type: application/json\r\n");
      client.print(msgBody);

      // Wait for reply and print it
      if(WAIT_FOR_REPLY_MSEC != 0) {
        delay(WAIT_FOR_REPLY_MSEC);
        Serial.println("Reply received:");
        while (client.available()) {
          char c = client.read();
          Serial.write(c);
        }
      }
    }

    // Close connection
    client.stop();

    // Note the time that the connection was made
    lastConnectionTime = millis();
  }
  else {
    Serial.println("connection failed");
  }
}

void printWifiStatus() {

  // Print IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("\nIP Address: ");
  Serial.println(ip);

  // Print RSSI
  long rssi = WiFi.RSSI();
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.println(" dBm");
}


