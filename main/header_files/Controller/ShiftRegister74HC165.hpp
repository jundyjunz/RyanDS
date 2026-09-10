#pragma once 
#include "../BuilderWarning.hpp"
#include "driver/gpio.h"
#define SHIFT_REGISTER_NUM 8

template<int SHIFT_REGISTER_COUNT>
class ShiftRegister74HC165 : Builder<ShiftRegister74HC165<SHIFT_REGISTER_COUNT>>{ 
    protected:
        gpio_num_t mySHLDPin; 
        gpio_num_t  myCLKPin; 
        gpio_num_t  myQHPin; 
        int myBitsRead; 
        uint8_t myBits[SHIFT_REGISTER_COUNT]={};
        void (*myCallBacks[SHIFT_REGISTER_COUNT*SHIFT_REGISTER_NUM])(int)={};
        bool readARegister(){   
            if(myBitsRead>=SHIFT_REGISTER_COUNT*SHIFT_REGISTER_NUM)return false;
            int theLevelOnPinQH =gpio_get_level(myQHPin);
            myBits[myBitsRead/SHIFT_REGISTER_NUM] |= theLevelOnPinQH << myBitsRead%SHIFT_REGISTER_NUM; 
            if(myCallBacks[myBitsRead]) myCallBacks[myBitsRead](theLevelOnPinQH);
            gpio_set_level(myCLKPin, 1); 
            gpio_set_level(myCLKPin, 0);
            myBitsRead++;
            return true;
        }

        void loadRegisters(){ 
            gpio_set_level(mySHLDPin, 0);  
            gpio_set_level(mySHLDPin, 1);   
            myBitsRead=0;
            for(int i=0; i< SHIFT_REGISTER_COUNT; i++) myBits[i]=0x0;
        } 

    public:
    ShiftRegister74HC165(){ 
        /* 
        gpio_config_t struct documentation: 
        Tells you how to initialize gpio pins.
        https://sourcevu.sysprogs.com/espressif/esp-idf/symbols/gpio_config_t  
        */
        mySHLDPin=GPIO_NUM_NC;
        myCLKPin=GPIO_NUM_NC; 
        myQHPin=GPIO_NUM_NC; 
        myBitsRead=0;
    }

   

    void readRegisters(){ 
        loadRegisters();
        while(readARegister()); 
    } 

    int getRegisterValue(int aIndex){ 
        int theByte=aIndex/SHIFT_REGISTER_NUM; 
        int theOffset=aIndex%SHIFT_REGISTER_NUM; 
        return myBits[theByte]>>theOffset & 0x01; 
    }

    ShiftRegister74HC165<SHIFT_REGISTER_COUNT>& buildCallBack(int aIndex, void (*aCallBack)(int) ){ 
        myCallBacks[aIndex]=aCallBack;
        return *this;
    }

    ShiftRegister74HC165<SHIFT_REGISTER_COUNT>& buildSHLDPin(gpio_num_t  aSHLDPin){ 
        gpio_set_direction(aSHLDPin, GPIO_MODE_OUTPUT);
        mySHLDPin=aSHLDPin;
        return *this;
    
    } 

    ShiftRegister74HC165<SHIFT_REGISTER_COUNT>& buildCLKPin(gpio_num_t aCLKPin){ 
        gpio_set_direction(aCLKPin, GPIO_MODE_OUTPUT);
        myCLKPin= aCLKPin;
        return *this;
    
    } 

    ShiftRegister74HC165<SHIFT_REGISTER_COUNT>& buildQHPin(gpio_num_t aQHPin){ 
        gpio_set_direction(aQHPin, GPIO_MODE_INPUT);
        myQHPin=aQHPin;
        return *this;
    
    } 
    
    ShiftRegister74HC165<SHIFT_REGISTER_COUNT>& buildInit() override{ 
        BuilderWarning<ShiftRegister74HC165,3> theBuilderWarning; 
        theBuilderWarning 
        .setRequired(mySHLDPin>0) 
        .setRequired(myCLKPin>0) 
        .setRequired(myQHPin>0) 
        .enforce();
        return *this;
    }

};