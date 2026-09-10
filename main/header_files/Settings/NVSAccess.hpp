#define DEFAULT_PARTITION "nvs" 
#define NVS_NAMESPACE "settings"

#pragma once
#include "nvs_flash.h"  
#include <string.h>

class NVSAccess{ 
    protected:   
    bool myNVSAccessIsOpen;
    nvs_handle_t myHandle;  

    bool doesKeyExist(const char* aKey){ 
        nvs_type_t theType;
        return nvs_find_key(myHandle, aKey, &theType)== ESP_OK;
    }

    NVSAccess(){myNVSAccessIsOpen=false;}  

    public: 
    static NVSAccess& getInstance(){ 
        static NVSAccess theAccess;
        return theAccess;
    }

    void openNVSAccess(){ 
        nvs_open(NVS_NAMESPACE, NVS_READWRITE, &myHandle);  
        myNVSAccessIsOpen=true;
    }

    void closeNVSAccess(){  
        nvs_close(myHandle); 
        myNVSAccessIsOpen=false;
    }

    esp_err_t getStr(const char* aKey, char* aOutBuffer, size_t* aOutSize){ 
        if(!doesKeyExist(aKey))nvs_set_str(myHandle, aKey, "");
        return nvs_get_str(myHandle, aKey, aOutBuffer, aOutSize); 
    }  

    esp_err_t setStr(const char* aKey, char* aStr){ 
        esp_err_t thePotentialError = nvs_set_str(myHandle, aKey, aStr);
        if(thePotentialError!=ESP_OK) return thePotentialError;
        return nvs_commit(myHandle);
    }
};