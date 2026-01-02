#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "MyTimerCam.h"
#include <driver/adc.h> // <--- Indispensable pour la méthode bas niveau

// ================= CONFIGURATION =================

// MQTT
const char* MQTT_SERVER   = "192.168.1.15";
const uint16_t MQTT_PORT  = 1883;
const char* TOPIC_IMAGE   = "nichoir/image";
const char* TOPIC_BAT     = "nichoir/batterie"; // <--- NOUVEAU

// Point d'Accès
const char* AP_SSID       = "NichoirConfig";
const char* AP_PASSWORD   = "12345678";

const unsigned long CAPTURE_INTERVAL_MS = 10000;

// Pin Batterie TimerCam (Interne)

#define BAT_ADC_PIN   38
#define BAT_HOLD_PIN  33
#define LED_PIN 2


// ================= OBJETS GLOBAUX =================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
MyTimerCam Camera;
Preferences preferences;
WebServer server(80);
DNSServer dnsServer;

String wifiSsid;
String wifiPass;
bool isStationMode = false;
unsigned long lastCapture = 0;
bool shouldRestart = false;
unsigned long restartStart = 0;

// ================= HTML DYNAMIQUE =================
String getPageHTML() {
  String html = R"rawliteral(<!DOCTYPE html>
  <html><head><meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Nichoir Config</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin: 20px; }
    select, input { padding: 10px; margin: 10px 0; width: 100%; box-sizing: border-box; }
    input[type=submit] { background-color: #4CAF50; color: white; border: none; font-size: 16px; }
  </style></head><body>
  <h2>Configuration Nichoir</h2>
  <form action="/save" method="POST">
    <label>Choisir le reseau :</label>
    <select name="ssid">)rawliteral";

  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<option value=''>Aucun reseau trouve</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>";
    }
  }

  html += R"rawliteral(
    </select>
    <br>
    <label>Ou entrer manuellement :</label>
    <input type="text" name="custom_ssid" placeholder="Nom du WiFi (si cache)">
    <br>
    <label>Mot de passe :</label>
    <input type="password" name="password" placeholder="Mot de passe">
    <br>
    <input type="submit" value="Enregistrer">
  </form>
  </body></html>)rawliteral";
  
  return html;
}

// ================= FONCTIONS BATTERIE =================


float readBatteryVoltage() {
  digitalWrite(BAT_HOLD_PIN, HIGH);
  delay(10); // laisse le pont se stabiliser

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_DB_11);

  long sum = 0;
  for(int i=0; i<20; i++) {
    sum += adc1_get_raw(ADC1_CHANNEL_2);
    delay(5);
  }
  float raw = sum / 20.0;
  Serial.printf("[DEBUG] Raw Low-Level: %.2f\n", raw);

  //digitalWrite(BAT_HOLD_PIN, LOW); // optionnel: coupe la mesure pour économiser

  if (raw < 1) return 0.0;

  float voltage = ((raw / 4095.0) * 3.3 * 2.0)-1; // *2 si diviseur 1/2
  return voltage;
}


void sendBattery() {
    float voltage = readBatteryVoltage();
    String msg = String(voltage, 2); // 2 décimales (ex: "4.12")
    
    Serial.print("Tension Batterie: "); Serial.print(msg); Serial.println(" V");
    mqttClient.publish(TOPIC_BAT, msg.c_str());
}

// ================= FONCTIONS CAMERA / MQTT =================

void sendImage(camera_fb_t* fb) {
  if (!fb || fb->len == 0) return;
  
  if (mqttClient.getBufferSize() < fb->len + 500) {
      mqttClient.setBufferSize(fb->len + 500);
  }

  Serial.printf("Envoi Image MQTT (%u bytes)... ", (unsigned)fb->len);
  bool ok = mqttClient.publish(TOPIC_IMAGE, (const uint8_t*)fb->buf, fb->len, false);
  Serial.println(ok ? "OK" : "ECHEC");
}

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("Connexion MQTT...");
    String clientId = "TimerCAM-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("OK");
    } else {
      Serial.print("Echec rc="); Serial.println(mqttClient.state());
    }
  }
}

// ================= WIFI & AP LOGIC =================

bool connectToSavedWiFi() {
  if (wifiSsid == "") return false;

  Serial.println("Tentative de connexion STA...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
    delay(500); Serial.print(".");
  }
  Serial.println();

  return (WiFi.status() == WL_CONNECTED);
}

void startConfigAP() {
  Serial.println(">>> MODE AP CONFIGURATION <<<");
  WiFi.disconnect(); 
  delay(100);
  WiFi.mode(WIFI_AP);
  
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("IP AP: "); Serial.println(WiFi.softAPIP());

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    shouldRestart = false;
    server.send(200, "text/html", getPageHTML());
  });

  server.on("/save", HTTP_POST, []() {
    String newSsid = server.arg("ssid");
    if (server.arg("custom_ssid") != "") newSsid = server.arg("custom_ssid");
    String newPass = server.arg("password");

    if (newSsid != "") {
      preferences.begin("wifi", false);
      preferences.putString("ssid", newSsid);
      preferences.putString("password", newPass);
      preferences.end();

      String resp = "<html><body style='font-family:sans-serif;text-align:center;margin-top:50px;'>";
      resp += "<h1>Sauvegarde OK</h1>";
      resp += "<p>Redemarrage dans 10s...</p>";
      resp += "<a href='/' style='color:red;'>ANNULER</a></body></html>";
      
      server.send(200, "text/html", resp);
      
      shouldRestart = true;
      restartStart = millis();
    } else {
      server.send(400, "text/plain", "Erreur: SSID manquant");
    }
  });

  server.onNotFound([]() {
    server.send(200, "text/html", getPageHTML());
  });

  server.begin();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
digitalWrite(LED_PIN, LOW);   // LED éteinte au démarrage

  
    // Active le circuit batterie / pont diviseur
  pinMode(BAT_HOLD_PIN, OUTPUT);
  digitalWrite(BAT_HOLD_PIN, HIGH);
  delay(50);

  // ---------------------------

  if (!Camera.begin(FRAMESIZE_SVGA, PIXFORMAT_JPEG, 1, 12)) {
    Serial.println("Erreur Camera!");
  }

  preferences.begin("wifi", true);
  wifiSsid = preferences.getString("ssid", "");
  wifiPass = preferences.getString("password", "");
  preferences.end();

  if (connectToSavedWiFi()) {
    isStationMode = true;
    Serial.println("Connecté au WiFi !");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  } else {
    isStationMode = false;
    startConfigAP();
  }
}

// ================= LOOP =================

void loop() {
  digitalWrite(LED_PIN, HIGH);

  if (isStationMode) {
    // --- MODE NORMAL ---
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastCapture >= CAPTURE_INTERVAL_MS) {
      lastCapture = now;
      
      // 1. On capture l'image
      camera_fb_t* fb = Camera.capture();
      
      if (fb) {
        if (mqttClient.connected()) {
            // 2. On envoie d'abord la batterie
            sendBattery();
            // 3. Ensuite l'image
            sendImage(fb);
        }
        Camera.freeFrame(fb);
      }
    }
  } else {
    // --- MODE AP ---
    dnsServer.processNextRequest();
    server.handleClient();

    if (shouldRestart && (millis() - restartStart > 10000)) {
      Serial.println("Redemarrage...");
      ESP.restart();
    }
  }
}