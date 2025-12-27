#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include "MyTimerCam.h" // attention doit être dans le dossier

// ================= CONFIGURATION =================

// MQTT (Raspberry Pi)
const char* MQTT_SERVER   = "192.168.1.15";  // IP du Pi
const uint16_t MQTT_PORT  = 1883;
const char* TOPIC_IMAGE   = "nichoir/image";

// Point d'Accès (Mode Configuration)
const char* AP_SSID       = "NichoirConfig";
const char* AP_PASSWORD   = "12345678";

// Intervalle de capture
const unsigned long CAPTURE_INTERVAL_MS = 10000; // 10 secondes

// ================= OBJETS GLOBAUX =================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
MyTimerCam Camera;
Preferences preferences;
WebServer server(80);

// Variables de fonctionnement
String wifiSsid;
String wifiPass;
bool isStationMode = false; // true = connecté au WiFi/MQTT, false = mode config AP
unsigned long lastCapture = 0;

// ================= PAGE HTML =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Config Nichoir</title>
    <style>
        body { font-family: Arial; text-align: center; margin-top: 50px; }
        input { padding: 10px; margin: 5px; width: 80%; }
        input[type=submit] { background-color: #4CAF50; color: white; border: none; cursor: pointer; }
    </style>
</head>
<body>
    <h1>Configuration WiFi du Nichoir</h1>
    <form action="/save" method="POST">
        <label>SSID (Nom du WiFi) :</label><br>
        <input type="text" name="ssid" placeholder="Nom du WiFi"><br>
        <label>Mot de passe :</label><br>
        <input type="password" name="password" placeholder="Mot de passe"><br><br>
        <input type="submit" value="Enregistrer & Redémarrer">
    </form>
</body>
</html>
)rawliteral";

// ================= FONCTIONS CAMERA / MQTT =================

void sendImage(camera_fb_t* fb) {
  if (!fb || fb->len == 0) return;

  Serial.printf("Envoi MQTT (%u bytes)... ", (unsigned)fb->len);
  
  // Augmenter le buffer si nécessaire juste avant l'envoi (sécurité)
  if (mqttClient.getBufferSize() < fb->len + 100) {
      mqttClient.setBufferSize(fb->len + 100);
  }

  bool ok = mqttClient.publish(TOPIC_IMAGE, (const uint8_t*)fb->buf, fb->len, false);

  if (ok) {
    Serial.println("OK !");
  } else {
    Serial.println("ECHEC.");
  }
}

void reconnectMQTT() {
  // On ne boucle pas indéfiniment ici pour ne pas bloquer le reste du code
  if (!mqttClient.connected()) {
    Serial.print("Tentative connexion MQTT...");
    String clientId = "TimerCAM-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connecté !");
    } else {
      Serial.print("Echec, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// ================= FONCTIONS WIFI / SERVER =================

bool connectToSavedWiFi() {
  if (wifiSsid.length() == 0) {
    Serial.println("Aucun SSID en mémoire.");
    return false;
  }

  Serial.print("Tentative de connexion à : ");
  Serial.println(wifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  unsigned long start = millis();
  // On essaye pendant 15 secondes max
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connecté ! IP : ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Impossible de se connecter au WiFi.");
    return false;
  }
}

void startConfigAP() {
  Serial.println(">>> Démarrage MODE CONFIGURATION (AP) <<<");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.print("Connectez-vous au WiFi : ");
  Serial.println(AP_SSID);
  Serial.print("Puis allez sur : http://");
  Serial.println(WiFi.softAPIP());

  // Route Affichage Formulaire
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  // Route Sauvegarde
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      String newSsid = server.arg("ssid");
      String newPass = server.arg("password");

      // Sauvegarde NVS
      preferences.begin("wifi", false);
      preferences.putString("ssid", newSsid);
      preferences.putString("password", newPass);
      preferences.end();

      server.send(200, "text/html", "<h1>Sauvegarde OK. Redemarrage...</h1>");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Erreur: Champs manquants");
    }
  });

  server.begin();
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== NICHOIR INTELLIGENT ===");

  // 1. Init Caméra (On le fait tout de suite pour vérifier le matériel)
  Serial.print("Init Caméra... ");
  if (!Camera.begin(FRAMESIZE_SVGA, PIXFORMAT_JPEG, 1, 12)) {
    Serial.println("ECHEC HARDWARE CAMERA !");
    while (true) delay(1000); // Stop forcé
  }
  Serial.println("OK.");

  // 2. Lecture Préférences WiFi
  preferences.begin("wifi", true); // Lecture seule
  wifiSsid = preferences.getString("ssid", "");
  wifiPass = preferences.getString("password", "");
  preferences.end();

  // 3. Tentative de connexion WiFi
  if (connectToSavedWiFi()) {
    // SUCCÈS : On passe en mode MQTT
    isStationMode = true;
    
    // Config MQTT
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setBufferSize(50000); // Taille buffer image

  } else {
    // ECHEC : On lance le mode AP pour configuration
    isStationMode = false;
    startConfigAP();
  }
}

// ================= LOOP =================

void loop() {
  if (isStationMode) {
    // --- MODE NORMAL (Connecté au Raspberry) ---
    
    // 1. Gestion connexion MQTT
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();

    // 2. Capture d'image périodique
    unsigned long now = millis();
    if (now - lastCapture >= CAPTURE_INTERVAL_MS) {
      lastCapture = now;
      
      Serial.println("Capture...");
      camera_fb_t* fb = Camera.capture();
      
      if (fb) {
        // Envoi seulement si MQTT est connecté
        if (mqttClient.connected()) {
           sendImage(fb);
        }
        Camera.freeFrame(fb);
      } else {
        Serial.println("Erreur Capture.");
      }
    }

  } else {
    // --- MODE CONFIGURATION (Point d'accès) ---
    server.handleClient();
  }
}