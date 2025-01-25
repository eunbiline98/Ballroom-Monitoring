#include <Arduino.h>
#include <WiFi.h>
#include <MQTT.h>
#include <SPI.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoJson.h>

// DHT22
#define DHTPIN 4
#define DHTTYPE DHT22

//========= Konfigurasi WiFi ============
const char *SSID = "xxxx";  // Nama jaringan WiFi
const char *PASS = "xxxxx"; // Password WiFi

//========= MQTT Config ============
const char *MQTT_SERVER = "cccc";
const char *MQTT_STATUS = "xxxxxxx"; // name_location/name_device/lwt
const char *MQTT_SENSOR = "xxxxx";   // name_location/name_device/stat
const char *WILL_MESSAGE = "Offline";

const char *MQTT_CLIENT_ID = "xxxx";  // create client id unique
const char *MQTT_USERNAME = "xxxxxx"; // user mqtt
const char *MQTT_PASSWORD = "xxxxxx"; // pass mqtt

String siteLocation = "name_site_location"; // sesuaikan posisi lokasi pemasangan end device

WiFiClient espClient;
MQTTClient client;
DHT dht(DHTPIN, DHTTYPE);

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
  // subscribe in here
  Serial.println("\nConnected to MQTT!");
  client.publish(MQTT_STATUS, "Online", true, 1);
}

void sendSensorData()
{
  // Membaca data suhu dan kelembaban dari sensor DHT
  float temperature = dht.readTemperature(); // Mengambil suhu dalam Celcius
  float humidity = dht.readHumidity();       // Mengambil kelembaban

  // Cek apakah pembacaan gagal
  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Failed to read from DHT sensor!");
    return; // Keluar dari fungsi jika pembacaan gagal
  }

  // Membuat objek JSON
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;
  jsonDoc["location"] = siteLocation; // Lokasi statis

  // Menyusun JSON menjadi string
  char buffer[128];
  size_t n = serializeJson(jsonDoc, buffer);
  serializeJson(jsonDoc, buffer);

  // Mengirim data JSON ke MQTT
  client.publish(MQTT_SENSOR, buffer, n);
  Serial.println("Sensor data sent: ");
  Serial.println(buffer);
}

void setup()
{
  Serial.begin(115200);
  dht.begin();
  setup_wifi();
  client.begin(MQTT_SERVER, 1883, espClient);
  client.setWill(MQTT_STATUS, WILL_MESSAGE, true, 1);
  connectToMqtt();
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
