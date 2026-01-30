#include "hx711.h"

HX711::HX711(uint8_t doutPin, uint8_t sckPin)
    : _dout(doutPin), _sck(sckPin) {}

void HX711::begin() {
    pinMode(_sck, OUTPUT);
    pinMode(_dout, INPUT);
    digitalWrite(_sck, LOW);
}

bool HX711::isReady() {
    return digitalRead(_dout) == LOW;
}

long HX711::readRaw() {
    const uint32_t start = millis();
    while (!isReady()) {  // wait until HX711 pulls DOUT low
        if (millis() - start >= 100) {
            return _hasLast ? _lastRaw : 0;
        }
        delay(1);
    }

    long value = 0;

    // 24-bit reading
    for (uint8_t i = 0; i < 24; i++) {
        digitalWrite(_sck, HIGH);
        delayMicroseconds(1);

        value = value << 1;
        if (digitalRead(_dout))
            value++;

        digitalWrite(_sck, LOW);
        delayMicroseconds(1);
    }

    // 1 extra pulse sets channel/gain (default: A 128x)
    digitalWrite(_sck, HIGH);
    delayMicroseconds(1);
    digitalWrite(_sck, LOW);
    delayMicroseconds(1);

    // sign extend from 24-bit to 32-bit
    if (value & 0x800000)
        value |= 0xFF000000;

    _lastRaw = value;
    _hasLast = true;
    return value;
}

void HX711::tare(uint16_t samples) {
    long sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += readRaw();
    }
    _offset = sum / samples;
}

float HX711::getWeight(uint16_t samples) {
    long sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += readRaw();
    }
    long avg = (sum / samples) - _offset;

    return avg / _scale;
}
