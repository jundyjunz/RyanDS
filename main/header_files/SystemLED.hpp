#pragma once
#include "led_strip.h"
#define RMT_FREQUENCY 10 * 1000 * 1000; // LED Strips use a custom protocol to be lit up.  
#define PIXEL_INDEX 0

class SystemLED{ 
    private: 
    led_strip_handle_t myLED; 
    bool myLEDIsOn;
    
    SystemLED(){ 
        led_strip_config_t theStripConfig = {};
        theStripConfig.strip_gpio_num = GPIO_NUM_48;
        theStripConfig.max_leds = 1;

        led_strip_rmt_config_t theRMTConfig = {};
        theRMTConfig.resolution_hz = RMT_FREQUENCY;

        led_strip_new_rmt_device(&theStripConfig, &theRMTConfig, &myLED); 
        myLEDIsOn=false;
    }

    public:  

    static SystemLED& getInstance(){ 
        static SystemLED theSystemLED; 
        return theSystemLED;
    } 

    void writeColor(uint8_t aRedIntensity, uint8_t aGreenIntensity, uint8_t aBlueIntensity){ 
        led_strip_set_pixel(myLED, PIXEL_INDEX, aRedIntensity, aGreenIntensity, aBlueIntensity);   // off
        led_strip_refresh(myLED); 
        if(aRedIntensity>0 || aGreenIntensity>0 || aBlueIntensity>0) myLEDIsOn=true;
    } 

    void turnOff(){ 
        led_strip_set_pixel(myLED, PIXEL_INDEX, 0, 0, 0);   // off
        led_strip_refresh(myLED); 
        myLEDIsOn=false;
    }  

    bool isOn(){return myLEDIsOn;}
};

