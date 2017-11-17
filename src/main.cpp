#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

// Load user credentials
#include "credentials.h"

// Pin definitions
#define SPEAKER_PIN D1
#define CALL_PIN D2
#define BUTTON_PIN D4
#define LOCK_PIN D5

// Parameters
const int mqttReconnectDelay = 5000;
const int wifiReconnectDelay = 5000;
const int lockOpenDelay = 500;
const int debugDelay = 1000;

// Variables
long lockCloseTime = 0;
char msg[50] = "";
long lastDebugTime = 0;

// MQTT client object
WiFiClient espClient;
PubSubClient client(espClient);

// Function declarations
void callback(char*, byte*, unsigned int);
void callReceived(void);
void reconnectMQTT(void);
void setupWifi(void);
void setupOTA(void);

void setup() {
  Serial.begin(115200);
  // Setup connectivity
  setupWifi();
  setupOTA();
  client.setServer(IPAddress(MQTT_IP), MQTT_PORT);
  client.setCallback(callback);

  // Setup pin modes
  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Setup interrupt
  attachInterrupt(CALL_PIN, callReceived, RISING);
}

void loop() {
  // Check for incoming OTA connection
  ArduinoOTA.handle();

  // Make sure we are connected and maintain broker connection
  if(!client.connected()){
    reconnectMQTT();
  }
  client.loop();

  // Is it time to "release" doorlock?
  if(millis() >= lockCloseTime){
    digitalWrite(LOCK_PIN, LOW);
  }
}

// Parse and handle communication TO the unit
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if(strcmp(topic, DOORLOCK_TOPIC) == 0){
    digitalWrite(LOCK_PIN, HIGH);
    Serial.println("Opening door!");
    lockCloseTime = millis() + lockOpenDelay;
  }
}

// Handle incoming "call" signal
void callReceived(){
  snprintf_P(msg, 50, "Incoming call!");
  client.publish("hass/lobby/doorbell", msg);
  //tone(SPEAKER_PIN, 440, 200);
}

// Handle reconnection to the MQTT broker on startup or on connection loss
void reconnectMQTT(){
  // Loop until connected
  while(!client.connected()){
    Serial.println("Attempting connection to MQTT broker");
    if(client.connect(HOSTNAME, MQTT_USER, MQTT_PASSWORD)){
      client.subscribe(DOORLOCK_TOPIC);
      Serial.println("Connected");
    }
    else{
      Serial.print("Failed, return code='");
      Serial.print(client.state());
      Serial.print("', trying again in ");
      Serial.print(mqttReconnectDelay/1000);
      Serial.println(" s.");
      delay(mqttReconnectDelay);
    }
  }
}

// Establish a wifi connection
void setupWifi(){
  // Select mode
  WiFi.mode(WIFI_STA);

  // Begin connection to network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait for connection to establish
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(wifiReconnectDelay);
    ESP.restart();
  }

  // Print IP
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Setup OTA functionality
void setupOTA(){
  // Set OTA parameters
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword((const char *)OTA_PASSWORD);

  // OTA event function calls
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  // Start OTA service
  ArduinoOTA.begin();
  return;
}
