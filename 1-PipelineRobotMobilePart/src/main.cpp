#include <Arduino.h>
#include <Wire.h>

#define WATERSENSOR_PIN 38

bool isWater;

void setup() {

    Serial.begin(115200);

    pinMode(WATERSENSOR_PIN, INPUT_PULLUP);
}

void loop() {
    
    isWater = digitalRead(WATERSENSOR_PIN);

    if (isWater)
    {
        Serial.print("Water:");
        Serial.println(isWater);
    }
    else
    {
        Serial.print("Water:");
        Serial.println(isWater);
    }
    delay(1000);
}

