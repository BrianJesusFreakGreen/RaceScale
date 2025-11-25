#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <pgmspace.h>

class SSD1306 {
public:
    SSD1306(uint8_t address = 0x3C, uint8_t w = 128, uint8_t h = 64);

    void begin();
    void clear();
    void display();

    void drawPixel(int x, int y, bool color = true);
    void drawChar(int x, int y, char c, bool color = true);
    void drawString(int x, int y, const char* str, bool color = true);

private:
    void sendCommand(uint8_t cmd);
    void sendCommands(const uint8_t* cmds, uint16_t count);
    void sendData(const uint8_t* data, uint16_t count);

    uint8_t _addr, _width, _height;
    uint16_t _bufferSize;

public:
    uint8_t* buffer;
};
