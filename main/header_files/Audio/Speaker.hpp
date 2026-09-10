#include "../BuilderWarning.hpp"
#include "driver/i2s_std.h"
#include <algorithm>
//https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html 
#define AUDIO_TRANSFER_CHUNK_SIZE 8192

class Speaker: Builder<Speaker>{ 
    private: 
    i2s_chan_handle_t myTxHandle;
    i2s_std_config_t myI2SConfig; 
    bool myTxHandleIsSet; 
    bool myI2sConfigIsSet;

    public: 
    
    Speaker(){ 
        myTxHandleIsSet = false; 
        myI2sConfigIsSet = false;
    } 

    Speaker& buildSpeakerConfig(size_t aFrequency, size_t aDMAFrameSize, size_t aDMABufferCt){ 
        i2s_chan_config_t theChannelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER); // first parameter is the i2s controller and the second parameter is who generates the clock. in this case, thatd be the esp32
        theChannelConfig.dma_desc_num = aDMABufferCt;      
        theChannelConfig.dma_frame_num = aDMAFrameSize;  
        i2s_new_channel(&theChannelConfig, &myTxHandle, NULL);
        myI2SConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(aFrequency);
        myI2SConfig.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
        myTxHandleIsSet = true; 
        return *this;
    } 

    Speaker& buildPinout(gpio_num_t aLRCPin, gpio_num_t aBCLKPin, gpio_num_t aDinPin){ 
        i2s_std_gpio_config_t theGpioConfig;
        
        theGpioConfig.mclk = I2S_GPIO_UNUSED;   // no master clock needed
        theGpioConfig.bclk = aBCLKPin;          // Serial clock for when the bits come in to the I2s Device
        theGpioConfig.ws   = aLRCPin;           // LRC (left right clock)/WS (word select), selects which channel audio is feeding into
        theGpioConfig.dout = aDinPin;           // the output pin of the esp32 is going to the input pin of the I2s module
        theGpioConfig.din  = I2S_GPIO_UNUSED;
        theGpioConfig.invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false, };

        myI2SConfig.gpio_cfg  = theGpioConfig;
        myI2sConfigIsSet = true;
        return *this;  
    }

    Speaker& buildInit(){ 
        BuilderWarning<Speaker, 2> theWarning; 
        theWarning 
        .setRequired(myTxHandleIsSet!=false)  
        .setRequired(myI2sConfigIsSet!=false)
        .enforce();
        i2s_channel_init_std_mode(myTxHandle, &myI2SConfig);
        i2s_channel_enable(myTxHandle);
        return *this;
    } 
    
    void writeToSpeaker(uint8_t* aBuffer, size_t aBytesToWrite, TickType_t aTicksToWait){  
        //dma does not guarentee all bytes will be written, so we enforce that with a while loop check
        size_t theTotalWritten = 0;
        while(theTotalWritten < aBytesToWrite){
            size_t theBytesWritten = 0;
            i2s_channel_write(myTxHandle, aBuffer + theTotalWritten, aBytesToWrite - theTotalWritten, &theBytesWritten, aTicksToWait);
            if(theBytesWritten == 0) break;  //avoid stalls if nothing is written
            theTotalWritten += theBytesWritten;
        }
    }
   

    void destroySpeaker(){ 
        i2s_channel_disable(myTxHandle);
        i2s_del_channel(myTxHandle); 
        myTxHandleIsSet=false;
        myI2sConfigIsSet=false;
    }
};