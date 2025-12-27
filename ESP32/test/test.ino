#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>  // <--- AJOUT POUR LE PORTAIL CAPTIF
#include "MyTimerCam.h"

// ================= CONFIGURATION =================

// MQTT
const char* MQTT_SERVER   = "192.168.0.101";
const uint16_t MQTT_PORT  = 1883;
const char* TOPIC_IMAGE   = "nichoir/image";

// Point d'Accès
const char* AP_SSID       = "NichoirConfig";
const char* AP_PASSWORD   = "12345678";

const unsigned long CAPTURE_INTERVAL_MS = 10000;

// ================= OBJETS GLOBAUX =================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
MyTimerCam Camera;
Preferences preferences;
WebServer server(80);
DNSServer dnsServer; // <--- Objet DNS

// Variables
String wifiSsid;
String wifiPass;
bool isStationMode = false;
unsigned long lastCapture = 0;
bool shouldRestart = false;
unsigned long restartStart = 0;

// ================= HTML DYNAMIQUE =================
// Cette fonction génère la page HTML en incluant la liste des réseaux scannés
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

  // Scan des réseaux pour remplir la liste
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

// ================= FONCTIONS CAMERA / MQTT =================

void sendImage(camera_fb_t* fb) {
  if (!fb || fb->len == 0) return;
  
  // Ajustement dynamique du buffer
  if (mqttClient.getBufferSize() < fb->len + 500) {
      mqttClient.setBufferSize(fb->len + 500);
  }

  Serial.printf("Envoi MQTT (%u bytes)... ", (unsigned)fb->len);
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

  // 1. Nettoyage et Force AP
  WiFi.disconnect(); 
  delay(100);
  WiFi.mode(WIFI_AP);
  
  // 2. Lancement AP
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("IP AP: "); Serial.println(WiFi.softAPIP());

  // 3. Configuration DNS (Portail Captif)
  // Redirige toutes les requêtes (*) vers l'IP de l'AP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  // 4. Routes Web
  
  // Route principale (affiche la liste scannée)
  server.on("/", HTTP_GET, []() {
    shouldRestart = false; // Annule le compte à rebours si on revient
    server.send(200, "text/html", getPageHTML());
  });

  // Route sauvegarde
  server.on("/save", HTTP_POST, []() {
    String newSsid = server.arg("ssid");
    // Si l'utilisateur a rempli le champ manuel, on l'utilise
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

  // Gestion du Portail Captif (Android/iOS vérifient souvent /generate_204)
  server.onNotFound([]() {
    server.send(200, "text/html", getPageHTML());
  });

  server.begin();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Init Caméra
  if (!Camera.begin(FRAMESIZE_SVGA, PIXFORMAT_JPEG, 1, 12)) {
    Serial.println("Erreur Camera!");
  }

  // Lecture Config
  preferences.begin("wifi", true);
  wifiSsid = preferences.getString("ssid", "");
  wifiPass = preferences.getString("password", "");
  preferences.end();

  // Tentative Connexion
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
  if (isStationMode) {
    // --- MODE NORMAL ---
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastCapture >= CAPTURE_INTERVAL_MS) {
      lastCapture = now;
      camera_fb_t* fb = Camera.capture();
      if (fb) {
        if (mqttClient.connected()) sendImage(fb);
        Camera.freeFrame(fb);
      }
    }
  } else {
    // --- MODE AP ---
    dnsServer.processNextRequest(); // Indispensable pour le portail captif
    server.handleClient();

    if (shouldRestart && (millis() - restartStart > 10000)) {
      Serial.println("Redemarrage...");
      ESP.restart();
    }
  }
}