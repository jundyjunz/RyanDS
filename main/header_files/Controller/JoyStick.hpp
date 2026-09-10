#include "esp_adc/adc_oneshot.h"


template<int JOYSTICK_CT>
class JoySticks{ 
    private:
        
    void (*myXCallBacks[JOYSTICK_CT])(int)={};
    void (*myYCallBacks[JOYSTICK_CT])(int)={};
    adc_channel_t myXAdcChannels[JOYSTICK_CT]; 
    adc_channel_t myYAdcChannels[JOYSTICK_CT];
    adc_oneshot_unit_handle_t myAdcHandle;
    adc_oneshot_chan_cfg_t  myChannelConfig;

    public: 
    
    JoySticks(adc_unit_t aAdcUnit = ADC_UNIT_1){ 
        adc_oneshot_unit_init_cfg_t theInitConfig = {};
        theInitConfig.unit_id = aAdcUnit;
        adc_oneshot_new_unit(&theInitConfig, &myAdcHandle);

        myChannelConfig.bitwidth = ADC_BITWIDTH_DEFAULT;   // 12-bit on S3
        myChannelConfig.atten = ADC_ATTEN_DB_12;           // ~0–3.1V range
    }  

    JoySticks& buildJoyStickPins(size_t aJoyStickIndex, adc_channel_t aXAdcChannel, adc_channel_t aYAdcChannel){ 
        adc_oneshot_config_channel(myAdcHandle, aXAdcChannel, &myChannelConfig); // GPIO4
        adc_oneshot_config_channel(myAdcHandle, aYAdcChannel, &myChannelConfig); // GPIO5 
        myXAdcChannels[aJoyStickIndex]=aXAdcChannel;
        myYAdcChannels[aJoyStickIndex]=aYAdcChannel; 
        return *this;
    }
    JoySticks& buildJoyStickCallBacks(size_t aJoyStickIndex, void (*aXCallBack)(int), void (*aYCallBack)(int)){ 
        myXCallBacks[aJoyStickIndex] = aXCallBack;
        myYCallBacks[aJoyStickIndex]  = aYCallBack;
        return *this;
    }

    void readJoyStickValues(size_t aJoyStickIndex){ 
        int theXValue = 0;
        int theYValue = 0;
        adc_oneshot_read(myAdcHandle, myXAdcChannels[aJoyStickIndex], &theXValue); 
        adc_oneshot_read(myAdcHandle, myYAdcChannels[aJoyStickIndex], &theYValue);
        if(myXCallBacks[aJoyStickIndex]) myXCallBacks[aJoyStickIndex](theXValue); 
        if(myYCallBacks[aJoyStickIndex]) myYCallBacks[aJoyStickIndex](theYValue); 
    } 

    void destroyJoySticks(){ 
        adc_oneshot_del_unit(myAdcHandle);
    }

};


