#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "MyTimerCam.h"




// ------------ CONFIG WIFI / MQTT ------------
const char* WIFI_SSID     = "electroProjectWifi";
const char* WIFI_PASSWORD = "B1MesureEnv";

const char* MQTT_SERVER   = "192.168.2.39";  // IP du Pi
const uint16_t MQTT_PORT  = 1883;

const char* TOPIC_IMAGE   = "nichoir/image";

// ------------ OBJETS GLOBAUX ------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);
MyTimerCam Camera;

// ------------ WIFI ------------
void setupWiFi() {
  Serial.print("Connexion WiFi à ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connecté !");
  Serial.print("IP locale: ");
  Serial.println(WiFi.localIP());
}

// ------------ MQTT ------------
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connexion MQTT...");
    String clientId = "TimerCAM-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("OK");
    } else {
      Serial.print("ECHEC, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" -> nouvelle tentative dans 2s");
      delay(2000);
    }
  }
}

// ------------ ENVOI IMAGE ------------
void sendImage(camera_fb_t* fb) {
  if (!fb || fb->len == 0) {
    Serial.println("Frame buffer vide !");
    return;
  }

  Serial.printf("Envoi image MQTT, taille = %u bytes\n", (unsigned)fb->len);

  bool ok = mqttClient.publish(
      TOPIC_IMAGE,
      (const uint8_t*)fb->buf,
      fb->len,
      false
  );

  if (ok) {
    Serial.println("Image envoyée avec succès !");
  } else {
    Serial.println("ERREUR envoi MQTT (taille trop grande ?)");
  }
}

// ------------ SETUP ------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== TimerCAM → MQTT (image complète) ===");

  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize0);   // par ex. 20 kB, > 14491
  mqttClient.setBufferSizec0);   // par ex. 20 kB, > 14491


  // Init caméra
  Serial.println("Init caméra...");
  if (!Camera.begin(FRAMESIZE_SVGA, PIXFORMAT_JPEG, 1, 12)) {
    // Astuce : FRAMESIZE_SVGA ou XGA donne des images plus petites que UXGA
    Serial.println("Camera Init Fail");
    while (true) { delay(1000); }
  }
  Serial.println("Camera Init Success");
}

// ------------ LOOP ------------
unsigned long lastCapture = 0;
const unsigned long CAPTURE_INTERVAL_MS = 10000; // 1 image toutes les 10s

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastCapture >= CAPTURE_INTERVAL_MS) {
    lastCapture = now;

    Serial.println("Capture image...");
    camera_fb_t* fb = Camera.capture();
    if (fb) {
      Serial.printf("Taille image: %u bytes\n", (unsigned)fb->len);
      sendImage(fb);
      Camera.freeFrame(fb);
    } else {
      Serial.println("Echec capture !");
    }
  }
}
