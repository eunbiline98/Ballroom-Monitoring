#include <Arduino.h>
#include <WiFi.h>
#include <MQTT.h>
#include <PZEM004Tv30.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>

//========= Konfigurasi WiFi ============
const char *SSID = "xxxx";  // Nama jaringan WiFi
const char *PASS = "xxxxx"; // Password WiFi

//========= MQTT Config ============
const char *MQTT_SERVER = "xxxx";
const char *MQTT_STATUS = "xxxxxxx"; // name_location/name_device/lwt
const char *MQTT_RELAY = "xxxxx";    // name_location/name_device/relay
const char *MQTT_SENSOR = "xxxxx";   // name_location/name_device/sensor
const char *WILL_MESSAGE = "Offline";

const char *MQTT_CLIENT_ID = "xxxx";  // create client id unique
const char *MQTT_USERNAME = "xxxxxx"; // user mqtt
const char *MQTT_PASSWORD = "xxxxxx"; // pass mqtt

String siteLocation = "name_site_location"; // sesuaikan posisi lokasi pemasangan end device

WiFiClient espClient;
MQTTClient client;

// Setup PZEM-004T v3.0
#if defined(ESP32)
PZEM004Tv30 pzem(Serial2, 16, 17);
#else
PZEM004Tv30 pzem(Serial2);
#endif

// Setup DHT
#define DHTPIN 4      // Pin where the DHT sensor is connected
#define DHTTYPE DHT22 // DHT 22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// Setup Relay
#define RELAY1_PIN 23
#define RELAY2_PIN 22
#define RELAY3_PIN 21
#define RELAY4_PIN 19

void setup_wifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(250);
  }
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed!");
    delay(5000);
    ESP.restart();
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address:  ");
  Serial.println(WiFi.macAddress());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void connectToMqtt()
{
  Serial.print("Connecting to MQTT...");
  while (!client.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD))
  {
    Serial.print(".");
    delay(1000);
  }
  client.subscribe(MQTT_RELAY);
  Serial.println("\nConnected to MQTT!");
  client.publish(MQTT_STATUS, "Online", true, 1);
}

void sendSensorData()
{
  // Membaca data dari PZEM-004T v3.0
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();
  float pf = pzem.pf();
  float frequency = pzem.frequency();

  // Cek apakah pembacaan berhasil
  if (voltage < 0 || current < 0 || power < 0 || energy < 0 || pf < 0 || frequency < 0)
  {
    Serial.println("Failed to read from PZEM-004T!");
    return; // Keluar dari fungsi jika pembacaan gagal
  }

  // Membaca data dari DHT
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Cek apakah pembacaan DHT berhasil
  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("Failed to read from DHT sensor!");
    return; // Keluar dari fungsi jika pembacaan gagal
  }

  // Membuat objek JSON
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["voltage"] = voltage;
  jsonDoc["current"] = current;
  jsonDoc["power"] = power;
  jsonDoc["energy"] = energy;
  jsonDoc["pf"] = pf;
  jsonDoc["frequency"] = frequency;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["location"] = siteLocation;

  // Menyusun JSON menjadi string
  char buffer[256];
  size_t n = serializeJson(jsonDoc, buffer);

  // Mengirim data JSON ke MQTT
  client.publish(MQTT_SENSOR, buffer, n);
  Serial.println("Sensor data sent: ");
  Serial.println(buffer);
}

void controlRelay(String topic, String message)
{
  if (topic == MQTT_RELAY)
  {
    if (message == "RELAY1_ON")
    {
      digitalWrite(RELAY1_PIN, HIGH);
      Serial.println("Relay 1 ON");
    }
    else if (message == "RELAY1_OFF")
    {
      digitalWrite(RELAY1_PIN, LOW);
      Serial.println("Relay 1 OFF");
    }
    else if (message == "RELAY2_ON")
    {
      digitalWrite(RELAY2_PIN, HIGH);
      Serial.println("Relay 2 ON");
    }
    else if (message == "RELAY2_OFF")
    {
      digitalWrite(RELAY2_PIN, LOW);
      Serial.println("Relay 2 OFF");
    }
    else if (message == "RELAY3_ON")
    {
      digitalWrite(RELAY3_PIN, HIGH);
      Serial.println("Relay 3 ON");
    }
    else if (message == "RELAY3_OFF")
    {
      digitalWrite(RELAY3_PIN, LOW);
      Serial.println("Relay 3 OFF");
    }
    else if (message == "RELAY4_ON")
    {
      digitalWrite(RELAY4_PIN, HIGH);
      Serial.println("Relay 4 ON");
    }
    else if (message == "RELAY4_OFF")
    {
      digitalWrite(RELAY4_PIN, LOW);
      Serial.println("Relay 4 OFF");
    }
  }
}

void messageReceived(String &topic, String &payload)
{
  Serial.println("Message received on topic: " + topic + " - " + payload);
  controlRelay(topic, payload);
}

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.begin(MQTT_SERVER, 1883, espClient);
  client.setWill(MQTT_STATUS, WILL_MESSAGE, true, 1);
  client.onMessage(messageReceived);
  connectToMqtt();

  // Inisialisasi DHT
  dht.begin();

  // Inisialisasi Relay
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  digitalWrite(RELAY4_PIN, LOW);
}

void loop()
{
  if (!client.connected())
  {
    connectToMqtt();
  }
  client.loop();

  // Send sensor data every 5 seconds
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 5000)
  {
    lastSendTime = millis();
    sendSensorData();
  }
}