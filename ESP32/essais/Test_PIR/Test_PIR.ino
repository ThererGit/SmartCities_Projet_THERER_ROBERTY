#define INPUT_PIN 4    //cf câble blanc
#define LED_PIN   13  

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {

  int state = digitalRead(INPUT_PIN);

  if(state == HIGH) {
    Serial.println("Signal détecté !");
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("Pas de signal");
    digitalWrite(LED_PIN, LOW);
  }

  delay(500); // petit délai pour éviter la spam
}