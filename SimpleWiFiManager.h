#ifndef WiFiManager_h
#define WiFiManager_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

#define WM_DEBUG // Comment to disable debug

#ifdef WM_DEBUG
#define WM_DBG(...) Serial.print(__VA_ARGS__)
#define WM_DBGLN(...) Serial.println(__VA_ARGS__)
#else
#define WM_DBG(...)
#define WM_DBGLN(...)
#endif

class WiFiManager {
private:
    DNSServer dns;
    ESP8266WebServer srv;
    bool _save = false;
    bool _conn = false;
    unsigned long _to = 180000;
    unsigned long _start = 0;
    const char* _ap = "ESP-CFG";
    const char* _apPass = "config";

    struct Config {
        char ssid[32];
        char pass[64];
    };

    void handleRoot() {
        String h = F("<!DOCTYPE html><html><head><meta name=viewport content='width=300'>"
                   "<title>WiFi</title><style>"
                   "body{font-family:Arial,sans-serif;width:300px;margin:0 auto}"
                   "input,select{width:100%;margin:5px 0 15px;padding:8px}"
                   "button{background:#4CAF50;color:#fff;padding:10px;border:0}"
                   "</style></head><body><h2>WiFi Setup</h2>"
                   "<form method=post action=/save>"
                   "<label>SSID:</label><select name=s required>"
                   "<option value=''>Select</option>");
        
        int n = WiFi.scanNetworks();
        for(int i=0; i<n; i++) {
            h += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
        }
        
        h += F("</select><label>Password:</label>"
              "<input type=password name=p>"
              "<button type=submit>Save</button></form></body></html>");
        
        srv.send(200, "text/html", h);
    }

    void handleSave() {
        if(srv.hasArg("s") && srv.arg("s").length() > 0) {
            Config c;
            strncpy(c.ssid, srv.arg("s").c_str(), sizeof(c.ssid));
            strncpy(c.pass, srv.arg("p").c_str(), sizeof(c.pass));
            
            EEPROM.begin(512);
            EEPROM.put(0, c);
            EEPROM.commit();
            EEPROM.end();
            
            _save = true;
            srv.send(200, "text/plain", "OK. Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            srv.send(400, "text/plain", "Bad Request");
        }
    }

    void startPortal() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(_ap, _apPass);
        dns.start(53, "*", WiFi.softAPIP());
        
        srv.on("/", std::bind(&WiFiManager::handleRoot, this));
        srv.on("/save", std::bind(&WiFiManager::handleSave, this));
        srv.begin();
        
        WM_DBG("AP IP: ");
        WM_DBGLN(WiFi.softAPIP());
    }

    bool loadConfig() {
        Config c;
        EEPROM.begin(512);
        EEPROM.get(0, c);
        EEPROM.end();
        
        if(c.ssid[0] == 0) return false;
        
        WiFi.persistent(true);
        WiFi.begin(c.ssid, c.pass);
        
        for(int i=0; i<20; i++) {
            if(WiFi.status() == WL_CONNECTED) {
                _conn = true;
                WiFi.persistent(false);
                WM_DBG("Connected to ");
                WM_DBGLN(c.ssid);
                return true;
            }
            delay(500);
            WM_DBG(".");
        }
        
        WiFi.persistent(false);
        return false;
    }

public:
    WiFiManager() : srv(80) {}

    void setTimeout(unsigned long sec) {
        _to = sec * 1000;
    }

    void setAP(const char* name, const char* pass) {
        _ap = name;
        _apPass = pass;
    }

    bool autoConnect() {
        if(loadConfig()) return true;
        
        WM_DBGLN("Starting config portal");
        startPortal();
        _start = millis();
        
        while(millis() - _start < _to) {
            dns.processNextRequest();
            srv.handleClient();
            delay(10);
            if(_save) break;
        }
        
        WiFi.mode(WIFI_STA);
        return _save;
    }
};

#endif