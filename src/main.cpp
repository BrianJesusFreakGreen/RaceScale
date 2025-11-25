#include <Arduino.h>
#include "ssd1306.h"
#include "hx711.h"

HX711 lfScale(D6,D3);
SSD1306 oled(0x3C, 128, 64);
std::string FormatWeight(float weight,int size);
void UpdateWeights();
float lfWeight, rfWeight,lrWeight, rrWeight;
float offset = 4180;
float scaleFactor = offset / 178 * 453.5924;

void setup() {
  Wire.begin(); // SDA, SCL for ESP32
  oled.begin();
 
  


  Serial.begin(115200);
  delay(2000);
  Serial.println("Scanning...");
  lfScale.begin();
  lfScale.tare();
  lfScale.setScale(scaleFactor);
  

    
 
}

void UpdateWeights(){
  oled.clear();
  long r = lfScale.readRaw();

    if (r == LONG_MIN) {
        Serial.println("Read failed (HX711 not ready)");
    } else {
        float net = r - offset;
        float lbs = net / scaleFactor;
        

        lfWeight = lfScale.getWeight();
    }
    
  
  float lfPercent, rfPercent, lrPercent, rrPercent = 0;
  oled.drawString(0,0,(FormatWeight(lfWeight,16) + "lbs").c_str()); 
  oled.drawString(0,12,(FormatWeight(lfPercent,8) + "%").c_str()); 
  oled.drawString(80,0,(FormatWeight(rfWeight,16) + "lbs").c_str());
  oled.drawString(80,12,(FormatWeight(rfPercent,8) + "%").c_str()); 
  oled.drawString(0,44,(FormatWeight(lrWeight,16) + "lbs").c_str()); 
  oled.drawString(0,56,(FormatWeight(lrPercent,8) + "%").c_str()); 
  oled.drawString(80,44,(FormatWeight(rrWeight,16) + "lbs").c_str());
  oled.drawString(80,56,(FormatWeight(rrPercent,8) + "%").c_str());
  oled.drawString(37,28,(FormatWeight(325,8) + "lbs").c_str()); 
  oled.display();
}

std::string FormatWeight(float weight,int size){
  char buf[size];
  snprintf(buf, sizeof(buf), "%3.2f", weight);
  return std::string(buf);
}

void loop() {
  /*for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("FOUND device at 0x%02X\n", addr);
    }
  }*/
 
  UpdateWeights();
  delay(200);
}
