#pragma once

class RgbService
{
public:
        static void start();
        static void stop();
        static void setRgb(int mode, int brightness, int speed, int r, int g, int b);

};
