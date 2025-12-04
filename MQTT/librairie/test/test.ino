#include <Arduino.h>
#include "MyTimerCam.h"

MyTimerCam Camera;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Init camera...");

    // Par ex : 2MP UXGA, JPEG, 1 frame buffer, qualité 10
    if (!Camera.begin(FRAMESIZE_UXGA, PIXFORMAT_JPEG, 1, 10)) {
        Serial.println("Camera Init Fail");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Camera Init Success");
}

void loop() {
    camera_fb_t* fb = Camera.capture();
    if (fb) {
        Serial.printf("pic size: %u bytes\n", fb->len);
        Camera.freeFrame(fb);
    } else {
        Serial.println("Capture failed");
    }

    delay(1000);  // une photo par seconde
}
