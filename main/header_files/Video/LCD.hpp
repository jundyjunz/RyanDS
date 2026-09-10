#pragma once
#include "../BuilderWarning.hpp"
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_lcd_panel_io.h"       // esp_lcd_new_panel_io_spi(), IO layer
#include "esp_lcd_panel_ops.h"      // esp_lcd_panel_draw_bitmap(), init/reset/etc.
#include "esp_lcd_panel_vendor.h"   // esp_lcd_new_panel_st7789() and other vendor-specific panel drivers  
#include "driver/gpio.h"
#include "../Circular/CircularReferenceBuffer.hpp"
#include "esp32s3/rom/cache.h"   // use this instead on older IDF, path may vary by exact version
#define ALIGNMENT 16
#define SPICLK 80 * 1000 * 1000 //60fps 
#define SPI_TRANSFER_QUEUE 2 
#define ROWS_TO_TRIGGER_DMA 64 
#define MEMORY_ALLOCATION_STRATEGY MALLOC_CAP_DMA
//LCD documentation --> https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/lcd/spi_lcd.html
//New jpeg documentation --> https://developer.espressif.com/blog/2025/09/esp-new-jpeg-introduction/

class LCD : Builder<LCD>{ 
    private:
    size_t myWidth;
    size_t myHeight;
    size_t myProtocolMultiplier;
    esp_lcd_panel_handle_t myLCD;
    esp_lcd_panel_io_handle_t myLCDIO;
    bool myDecoderIsBuilt; 
    bool myLCDIsBuilt; 
    uint8_t* myInBuffer;
    uint8_t* myOutBuffer; 
    size_t myJpegBlockSize;
    jpeg_dec_handle_t myDecoder;
    jpeg_dec_io_t myCallBackHandle;
    jpeg_dec_header_info_t myOutInfoHandle;  
    size_t myXOffset; 
    size_t myYOffset;
    jpeg_dec_config_t myDecodeConfig;
    SemaphoreHandle_t myDMASemaphore; 

    static bool IRAM_ATTR onDMADone(esp_lcd_panel_io_handle_t aLCDIO, esp_lcd_panel_io_event_data_t* aEventData, void* aUserCtx){ // marked as IRAM SAFE
        LCD* theLCD = static_cast<LCD*>(aUserCtx);
        BaseType_t theHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(theLCD->myDMASemaphore, &theHigherPriorityTaskWoken);
        return theHigherPriorityTaskWoken == pdTRUE; 
    }

    void buildSPIBus(gpio_num_t aSCLPin, gpio_num_t aSDAPin){ 
        spi_bus_config_t theSPIBusConfig= {}; 
        theSPIBusConfig.mosi_io_num = aSDAPin;   //MOSI
        theSPIBusConfig.miso_io_num = -1;        //MISO
        theSPIBusConfig.sclk_io_num = aSCLPin;   //SCLK
        theSPIBusConfig.quadwp_io_num = -1; //irrelevant to st7789 no quad spi
        theSPIBusConfig.quadhd_io_num = -1; //irrelevant to st7789 no quad spi
        theSPIBusConfig.max_transfer_sz = static_cast<int>(myWidth* myHeight * myProtocolMultiplier);
        
        spi_bus_initialize(SPI2_HOST, &theSPIBusConfig, SPI_DMA_CH_AUTO); //SPI2_host chosen since it is general purpose for the spi_host_device_t type 
    }
    void buildLCDIO(gpio_num_t aDCPin, gpio_num_t aCSPin){  
        esp_lcd_panel_io_spi_config_t theIOConfig = {};
        theIOConfig.dc_gpio_num = aDCPin;
        theIOConfig.cs_gpio_num = aCSPin; //only using one screen here, no chip select.
        theIOConfig.pclk_hz = SPICLK;
        theIOConfig.lcd_cmd_bits =8; //bit width of command bytes sent to channel, always 8 for st7789
        theIOConfig.lcd_param_bits = 8; // bit width following a command, also always 8.
        theIOConfig.spi_mode = 0; //clock idles low sampled on rising edge, just how the st7789 works.
        theIOConfig.trans_queue_depth = SPI_TRANSFER_QUEUE; 
        // Attach the LCD to the SPI bus
        esp_lcd_new_panel_io_spi(SPI2_HOST, &theIOConfig, &myLCDIO);

        //setting up callback for dma finish
        myDMASemaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(myDMASemaphore);
        esp_lcd_panel_io_callbacks_t theCallbacks = {};
        theCallbacks.on_color_trans_done = &LCD::onDMADone;
        esp_lcd_panel_io_register_event_callbacks(myLCDIO, &theCallbacks, this); // 'this' becomes aUserCtx
    }
    
    void buildLCD(gpio_num_t aResetPin){   
        esp_lcd_panel_dev_config_t thePanelConfig = {};
        thePanelConfig.reset_gpio_num = aResetPin;
        thePanelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB; // pixel order. use RGB for st7798
        thePanelConfig.bits_per_pixel = 16; // how many bits per pixel in the buffer we give. matches with teh DMA transfer alignment.

        // Create LCD panel handle for ST7789, with the SPI IO device handle
        esp_lcd_new_panel_st7789(myLCDIO, &thePanelConfig, &myLCD); 
        esp_lcd_panel_reset(myLCD);
        esp_lcd_panel_init(myLCD);
        esp_lcd_panel_disp_on_off(myLCD, true); 
        esp_lcd_panel_invert_color(myLCD, true);
        esp_lcd_panel_swap_xy(myLCD, true); 
        esp_lcd_panel_mirror(myLCD, true, false);
    }

    bool rebootDecoder(){ 
        jpeg_dec_close(myDecoder); 
        jpeg_dec_open(&myDecodeConfig, &myDecoder);
        return false;
    }

    bool drawBlock( size_t* aStagedPixels ,size_t* aRowsProcessed){ 

        if (jpeg_dec_process(myDecoder, &myCallBackHandle) != ESP_OK) return rebootDecoder(); 
        
        myCallBackHandle.outbuf+=myCallBackHandle.out_size;
        
        *aStagedPixels += (myCallBackHandle.out_size / myProtocolMultiplier); 
        size_t theRowsToProcess = *aStagedPixels/myWidth;
        if(theRowsToProcess < ROWS_TO_TRIGGER_DMA) return true;

        xSemaphoreTake(myDMASemaphore, portMAX_DELAY); 
        CircularReferenceBuffer::swapBytePointers(&myInBuffer, &myOutBuffer);
        esp_lcd_panel_draw_bitmap(
            myLCD, 
            0,*aRowsProcessed,
            myWidth, *aRowsProcessed+theRowsToProcess,  
            myOutBuffer
        );  
        *aRowsProcessed+=theRowsToProcess;
        *aStagedPixels-=(theRowsToProcess*myWidth);
        myCallBackHandle.outbuf=myInBuffer;
        return true;
    }
    
    public:
    LCD(size_t aWidth, size_t aHeight, size_t aProtocolMultiplier=2) :  
    myWidth(aWidth),  
    myHeight(aHeight),  
    myProtocolMultiplier(aProtocolMultiplier),
    myLCD(NULL),
    myLCDIO(NULL),
    myDecoderIsBuilt(false), 
    myLCDIsBuilt(false){ 
        myJpegBlockSize = aWidth*ROWS_TO_TRIGGER_DMA*aProtocolMultiplier;
        myInBuffer = static_cast<uint8_t*>(heap_caps_aligned_calloc(ALIGNMENT, 1,myJpegBlockSize, MEMORY_ALLOCATION_STRATEGY));
        myOutBuffer = static_cast<uint8_t*>(heap_caps_aligned_calloc(ALIGNMENT, 1, myJpegBlockSize, MEMORY_ALLOCATION_STRATEGY));  
    } // x2 for format RGB656_LE   

    LCD& buildDecodeEngine( jpeg_rotate_t aRotation=JPEG_ROTATE_0D){ 
        myDecodeConfig = DEFAULT_JPEG_DEC_CONFIG();
        myDecodeConfig.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
        myDecodeConfig.rotate = aRotation;
        myDecodeConfig.block_enable=true;

        jpeg_dec_open(&myDecodeConfig, &myDecoder); 
        
        myDecoderIsBuilt=true; 
        return *this;
    }
    
    LCD& buildLCDAll(gpio_num_t aSCLPin, gpio_num_t aSDAPin, gpio_num_t aResetPin, gpio_num_t aDCPin, gpio_num_t aCSPin){ 
        if(!myDecoderIsBuilt) return *this; 
        
        buildSPIBus(aSCLPin, aSDAPin); 
        buildLCDIO(aDCPin, aCSPin); 
        buildLCD(aResetPin);

        myLCDIsBuilt=true;
        return *this;
    } 

   void drawJpeg(uint8_t* aInputBuffer, size_t aLen){
        if (!myLCDIsBuilt || !myDecoderIsBuilt) return;
        
        myCallBackHandle.inbuf = aInputBuffer;
        myCallBackHandle.inbuf_len = aLen;
        myCallBackHandle.outbuf = myInBuffer;
        int theBlocksToProcess = 0;
        size_t theStagedPixels = 0;
        size_t theRowsProcessed = 0; 
        
        if (jpeg_dec_parse_header(myDecoder, &myCallBackHandle, &myOutInfoHandle) != JPEG_ERR_OK) return; 
        if (jpeg_dec_get_process_count(myDecoder, &theBlocksToProcess)!=JPEG_ERR_OK) return;
        
        for (int i = 0; i < theBlocksToProcess; i++) if(!drawBlock(&theStagedPixels, &theRowsProcessed)) return;

        if(theStagedPixels==0) return; 
        
        xSemaphoreTake(myDMASemaphore, portMAX_DELAY); // dma/cpu contention can cause silent crashes. Use a semaphore to sleep the thread until dma is done
        CircularReferenceBuffer::swapBytePointers(&myInBuffer, &myOutBuffer);
        esp_lcd_panel_draw_bitmap( 
            myLCD,
            0, theRowsProcessed,
            myWidth,theRowsProcessed+(theStagedPixels/myWidth),  
            myOutBuffer
        ); 
    }

    LCD& buildInit(){ 
        BuilderWarning<LCD, 2> theWarning; 
        theWarning 
        .setRequired(myDecoderIsBuilt!=false)  
        .setRequired(myLCDIsBuilt!=false)
        .enforce();
        return *this;
    } 

    void destroyLCD(){  
        if (myDecoderIsBuilt) jpeg_dec_close(myDecoder); 
        if (myLCDIsBuilt) esp_lcd_panel_del(myLCD);        // tears down the ST7789 panel driver
        if (myLCDIsBuilt) esp_lcd_panel_io_del(myLCDIO);
        if (myLCDIsBuilt) spi_bus_free(SPI2_HOST);
        heap_caps_free(myInBuffer); 
        heap_caps_free(myOutBuffer);
        myLCD = NULL;
        myLCDIO = NULL;
        myLCDIsBuilt = false;
    }
};
