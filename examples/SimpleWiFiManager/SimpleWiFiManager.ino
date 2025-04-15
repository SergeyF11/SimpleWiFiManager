#include "WiFiManager.h"

WiFiManager wifiManager;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Добавляем пользовательские параметры
  wifiManager.addParameter("hostname", "Device Hostname", "esp8266-device");
  wifiManager.addParameter("mqtt_server", "MQTT Server", "mqtt.example.com");
  wifiManager.addParameter("mqtt_port", "MQTT Port", "1883");
  wifiManager.addParameter("api_key", "API Key", "", "password");

  // Пытаемся подключиться или запустить портал конфигурации
  if (!wifiManager.autoConnect()) {
    Serial.println("Failed to connect and config portal timeout");
    ESP.restart();
  }

  // Выводим полученные параметры
  Serial.println("Connected to WiFi!");
  Serial.print("Hostname: ");
  Serial.println(wifiManager.getParam("hostname"));
  Serial.print("MQTT Server: ");
  Serial.println(wifiManager.getParam("mqtt_server"));
  Serial.print("MQTT Port: ");
  Serial.println(wifiManager.getParam("mqtt_port"));
}

void loop() {
  // Ваш основной код
  if (!wifiManager.isConnected()) {
    Serial.println("WiFi disconnected, restarting...");
    delay(1000);
    ESP.restart();
  }
}