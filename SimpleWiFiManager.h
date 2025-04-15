#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <vector>

#define CONFIG_PORTAL_TIMEOUT 180 // 3 минуты таймаут для портала
#define MAX_PARAM_LENGTH 64
#define EEPROM_SIZE 512

const byte DNS_PORT = 53;
const char* AP_SSID = "ESP8266-Config";
const char* AP_PASSWORD = "configureme";

struct WiFiConfig {
  char ssid[32];
  char password[64];
};

struct CustomParam {
  String name;
  String value;
  String label;
  String type;
};

class WiFiManager {
private:
  DNSServer dnsServer;
  ESP8266WebServer webServer;
  std::vector<CustomParam> customParams;
  String hostname = "esp8266";
  unsigned long configPortalStart = 0;
  bool _shouldSaveConfig = false;
  bool _connected = false;

  void setupConfigPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    delay(500); // Даем AP время подняться
    
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    webServer.on("/", std::bind(&WiFiManager::handleRoot, this));
    webServer.on("/save", std::bind(&WiFiManager::handleSave, this));
    webServer.on("/scan", std::bind(&WiFiManager::handleScan, this));
    webServer.onNotFound(std::bind(&WiFiManager::handleNotFound, this));
    webServer.begin();
    
    Serial.print("Config Portal IP: ");
    Serial.println(WiFi.softAPIP());
  }

  void handleRoot() {
    String html = "<html><head><title>WiFi Configuration</title>";
    html += "<style>body{font-family:Arial,sans-serif;margin:20px}";
    html += "form{max-width:400px;margin:0 auto}";
    html += "input,select{width:100%;padding:8px;margin:5px 0 15px;box-sizing:border-box}";
    html += "button{background:#4CAF50;color:white;padding:10px 15px;border:none;cursor:pointer}";
    html += "</style></head><body>";
    html += "<h2>WiFi Configuration</h2>";
    html += "<form method='post' action='/save'>";
    
    // Поля для WiFi
    html += "<label for='ssid'>WiFi SSID:</label>";
    html += "<select id='ssid' name='ssid' required>";
    html += "<option value=''>Select Network</option>";
    html += "</select><br>";
    
    html += "<label for='password'>Password:</label>";
    html += "<input type='password' id='password' name='password'><br>";
    
    // Пользовательские параметры
    for (const auto& param : customParams) {
      html += "<label for='" + param.name + "'>" + param.label + ":</label>";
      if (param.type == "password") {
        html += "<input type='password' id='" + param.name + "' name='" + param.name + "' value='" + param.value + "'><br>";
      } else {
        html += "<input type='text' id='" + param.name + "' name='" + param.name + "' value='" + param.value + "'><br>";
      }
    }
    
    html += "<button type='submit'>Save</button>";
    html += "</form>";
    html += "<script>fetch('/scan').then(r=>r.json()).then(networks=>{";
    html += "const select=document.getElementById('ssid');";
    html += "networks.forEach(n=>{const opt=document.createElement('option');";
    html += "opt.value=opt.textContent=n;select.appendChild(opt)})})</script>";
    html += "</body></html>";
    
    webServer.send(200, "text/html", html);
  }

  void handleSave() {
    if (webServer.hasArg("ssid")) {
      WiFiConfig config;
      strncpy(config.ssid, webServer.arg("ssid").c_str(), sizeof(config.ssid));
      strncpy(config.password, webServer.arg("password").c_str(), sizeof(config.password));
      
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.put(0, config);
      
      // Сохраняем пользовательские параметры
      for (auto& param : customParams) {
        if (webServer.hasArg(param.name)) {
          param.value = webServer.arg(param.name);
        }
      }
      saveCustomParams();
      
      EEPROM.end();
      _shouldSaveConfig = true;
      
      webServer.send(200, "text/html", "<html><body><h2>Configuration saved</h2><p>Device will now try to connect to " + 
                   String(config.ssid) + " and restart.</p></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      webServer.send(400, "text/plain", "Bad Request");
    }
  }

  void handleScan() {
    String json = "[";
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      if (i) json += ",";
      json += "\"" + WiFi.SSID(i) + "\"";
    }
    json += "]";
    webServer.send(200, "application/json", json);
  }

  void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += webServer.uri();
    message += "\nMethod: ";
    message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += webServer.args();
    message += "\n";
    for (uint8_t i = 0; i < webServer.args(); i++) {
      message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    }
    webServer.send(404, "text/plain", message);
  }

  void saveCustomParams() {
    int address = sizeof(WiFiConfig);
    for (const auto& param : customParams) {
      EEPROM.put(address, param.name.length());
      address += sizeof(int);
      for (char c : param.name) {
        EEPROM.put(address, c);
        address++;
      }
      
      EEPROM.put(address, param.value.length());
      address += sizeof(int);
      for (char c : param.value) {
        EEPROM.put(address, c);
        address++;
      }
    }
    EEPROM.commit();
  }

  void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    WiFiConfig config;
    EEPROM.get(0, config);
    
    if (config.ssid[0] != 0) {
      WiFi.persistent(true);
      WiFi.begin(config.ssid, config.password);
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        _connected = true;
        Serial.println("\nConnected to WiFi");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
      }
      WiFi.persistent(false);
    }
    
    // Загружаем пользовательские параметры
    int address = sizeof(WiFiConfig);
    while (address < EEPROM_SIZE) {
      int nameLength;
      EEPROM.get(address, nameLength);
      address += sizeof(int);
      
      if (nameLength <= 0 || nameLength > MAX_PARAM_LENGTH) break;
      
      String name;
      for (int i = 0; i < nameLength; i++) {
        char c;
        EEPROM.get(address + i, c);
        name += c;
      }
      address += nameLength;
      
      int valueLength;
      EEPROM.get(address, valueLength);
      address += sizeof(int);
      
      if (valueLength < 0 || valueLength > MAX_PARAM_LENGTH) break;
      
      String value;
      for (int i = 0; i < valueLength; i++) {
        char c;
        EEPROM.get(address + i, c);
        value += c;
      }
      address += valueLength;
      
      // Обновляем существующие параметры
      for (auto& param : customParams) {
        if (param.name == name) {
          param.value = value;
          break;
        }
      }
    }
    EEPROM.end();
  }

public:
  WiFiManager() : webServer(80) {}

  void addParameter(const String& name, const String& label, const String& defaultValue = "", const String& type = "text") {
    CustomParam param;
    param.name = name;
    param.label = label;
    param.value = defaultValue;
    param.type = type;
    customParams.push_back(param);
  }

  bool autoConnect() {
    loadConfig();
    
    if (!_connected) {
      Serial.println("Failed to connect to WiFi. Starting config portal...");
      setupConfigPortal();
      configPortalStart = millis();
      
      while (millis() - configPortalStart < CONFIG_PORTAL_TIMEOUT * 1000) {
        dnsServer.processNextRequest();
        webServer.handleClient();
        delay(10);
        
        if (_shouldSaveConfig) {
          break;
        }
      }
      
      WiFi.mode(WIFI_STA);
      dnsServer.stop();
      webServer.stop();
      
      if (_shouldSaveConfig) {
        return true;
      }
    }
    return _connected;
  }

  String getParam(const String& name) {
    for (const auto& param : customParams) {
      if (param.name == name) {
        return param.value;
      }
    }
    return "";
  }

  bool isConnected() {
    return _connected;
  }
};