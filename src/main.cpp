#include <Arduino.h>
#include "ssd1306.h"
#include "hx711.h"

HX711 scales[] = {{D6,D3}};
SSD1306 oled(0x3C, 128, 64);
std::string FormatWeight(float weight,int size);
void UpdateWeights();
float lfWeight, rfWeight,lrWeight, rrWeight, totalWeight, crossWeight;
float lfPercent, rfPercent, lrPercent, rrPercent;
float offset = 4180;
float weights[] = {0,0,0,0};
float percents[] = {0,0,0,0};
float scaleFactor = offset / 178 * 453.5924;

void setup() {
  Wire.begin(D4,D5); // SDA, SCL for ESP32
  oled.begin();
 
  


  Serial.begin(115200);
  delay(2000);
  Serial.println("Scanning...");

  for(int i = 0; i<(sizeof(scales)/sizeof(scales[0])); i++){
    scales[i].begin();
    scales[i].tare();
    scales[i].setScale(scaleFactor);
  }
  
  

    
 
}

void UpdateWeights(){
  oled.clear();
  totalWeight = 0;
  for(int i = 0; i < (sizeof(scales)/sizeof(scales[0])); i++){
  
  long r = scales[i].readRaw();
    if (r == LONG_MIN) {
        Serial.println("Read failed (HX711 not ready)");
        weights[i] = 0;
    } else {
        float net = r - offset;
        float lbs = net / scaleFactor;
        weights[i] = scales[i].getWeight(5);
        totalWeight += weights[i];
    }
  }
    
  crossWeight = (weights[0] + weights[3]) / totalWeight * 100;  
  
  for(int i = 0; i < (sizeof(weights)/sizeof(weights[0])); i++){
    percents[i] = weights[i] / totalWeight;
  }
  oled.drawString(0,0,(FormatWeight(weights[0],16) + "lbs").c_str()); 
  oled.drawString(0,11,(FormatWeight(percents[0],8) + "%").c_str()); 
  oled.drawString(80,0,(FormatWeight(weights[1],16) + "lbs").c_str());
  oled.drawString(80,11,(FormatWeight(percents[1],8) + "%").c_str()); 
  oled.drawString(0,47,(FormatWeight(weights[2],16) + "lbs").c_str()); 
  oled.drawString(0,56,(FormatWeight(percents[2],8) + "%").c_str()); 
  oled.drawString(80,47,(FormatWeight(weights[3],16) + "lbs").c_str());
  oled.drawString(80,56,(FormatWeight(percents[3],8) + "%").c_str());
  oled.drawString(37,26,(FormatWeight(totalWeight,8) + "lbs").c_str());
  oled.drawString(37, 37,(FormatWeight(crossWeight,8) + "%").c_str()); 
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
