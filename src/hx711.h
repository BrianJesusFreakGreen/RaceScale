#pragma once
#include <Arduino.h>

class HX711 {
public:
    HX711(uint8_t doutPin, uint8_t sckPin);

    void begin();
    bool isReady();
    long readRaw();
    void tare(uint16_t samples = 10);
    float getWeight(uint16_t samples = 3);

    void setScale(float scale) { _scale = scale; }
    float getScale() const { return _scale; }

private:
    uint8_t _dout, _sck;
    long _offset = 0;
    float _scale = 1.0;
    long _lastRaw = 0;
    bool _hasLast = false;
};
