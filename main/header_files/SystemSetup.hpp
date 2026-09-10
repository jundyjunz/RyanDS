#pragma once
#include "nvs_flash.h"  
#include "esp_event.h"      
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"  
#include <string.h>
#define CONNECTION_TIMEOUT_MS 15000
#define DNS_SERVER "8.8.8.8"
#define SERVER_CHANNELS 1
#define SERVER_MAX_CONNECTIONS 1

class SystemSetup{ 
    private: 

    enum class WifiMode{AP, STA, NONE};

    bool myNVSIsReady; 
    bool myNetifIsReady; 
    bool myEventLoopIsReady;
    esp_netif_t* myWifiHandle;
    EventBits_t myConnectionBit;
    EventGroupHandle_t myConnectionEventGroup;
    esp_event_handler_instance_t myWifiEventHandlerInstance;
    esp_event_handler_instance_t myIPEventHandlerInstance; 
    esp_netif_dns_info_t myDnsInfo={}; 
    wifi_config_t myWifiConfig = {};  
    WifiMode myWifiMode;

    void setServerConfig(const char* aNetworkName, const char* aNetworkPassword){  
        strcpy((char*)myWifiConfig.ap.ssid, aNetworkName ); 
        strcpy((char*)myWifiConfig.ap.password, aNetworkPassword);
        myWifiConfig.ap.ssid_len = strlen(aNetworkName);
        myWifiConfig.ap.channel = SERVER_CHANNELS;
        myWifiConfig.ap.max_connection = SERVER_MAX_CONNECTIONS;
        myWifiConfig.ap.authmode = WIFI_AUTH_WPA2_PSK;
        myWifiConfig.ap.pmf_cfg.required = true; 
    } 

    void setClientConfig(const char* aNetworkName, const char* aNetworkPassword){ 
        strcpy((char*)myWifiConfig.sta.ssid, aNetworkName );
        strcpy((char*)myWifiConfig.sta.password, aNetworkPassword );
        myWifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;   
    }

    void initWifi(WifiMode aMode){ 
        myWifiHandle = aMode==WifiMode::AP ? esp_netif_create_default_wifi_ap() : esp_netif_create_default_wifi_sta(); 
        wifi_init_config_t theConfig = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&theConfig);
    }

    void beginWifi(WifiMode aMode){ 
        esp_wifi_set_mode(aMode==WifiMode::AP ? WIFI_MODE_AP : WIFI_MODE_STA);
        esp_wifi_set_config(aMode==WifiMode::AP ? WIFI_IF_AP : WIFI_IF_STA, &myWifiConfig);
        esp_wifi_start();   
        myWifiMode=aMode;
    }

    void endWifi(){ 
        esp_wifi_stop();       
        esp_wifi_deinit();     
        esp_netif_destroy_default_wifi(myWifiHandle);  
        memset(&myWifiConfig,0, sizeof(myWifiConfig));  
        myWifiHandle=nullptr; 
        myWifiMode=WifiMode::NONE; 
    }

    static void SystemShutdown(){ 
        SystemSetup::getInstance().deInitWifiServer(); 
        SystemSetup::getInstance().deInitWifiClient();
        SystemSetup::getInstance().deinitEventLoop();  
        SystemSetup::getInstance().deinitNetif();
        SystemSetup::getInstance().deinitNVS();
    }

    SystemSetup(){ 
        myNVSIsReady = false; 
        myNetifIsReady = false; 
        myEventLoopIsReady = false; 
        myWifiHandle=nullptr; 
        myConnectionBit=BIT0;  
        myWifiMode = WifiMode::NONE;
        esp_register_shutdown_handler(&SystemShutdown);
    } 

    public: 
    static SystemSetup& getInstance(){ 
        static SystemSetup theSetup; 
        return theSetup;
    } 

    void initNVS(){ 
        if(myNVSIsReady) return; 
        nvs_flash_init(); 
        myNVSIsReady=true;
    } 

    void initEventLoop(){ 
        if(myEventLoopIsReady) return; 
        esp_event_loop_create_default();
        myEventLoopIsReady=true;
    } 

    void initNetif(){ 
        if(myNetifIsReady) return; 
        esp_netif_init(); 
        myNetifIsReady=true;
    } 

    void initWifiClient(const char* aNetworkName, const char* aNetworkPassword){  
        if(!(myNetifIsReady && myEventLoopIsReady && myNVSIsReady)) return;
        
        initWifi(WifiMode::STA);
        myConnectionEventGroup=xEventGroupCreate();

        //Init a handler to keep the connection alive
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, [](void* aArgs, esp_event_base_t aEventBase, int32_t aEventID, void* aData){ 
            if (aEventID == WIFI_EVENT_STA_START || aEventID == WIFI_EVENT_STA_DISCONNECTED)esp_wifi_connect(); 
        }, nullptr, &myWifiEventHandlerInstance);

        //Init a handler to start a DNS connection and set the event group to notify other tasks that wifi has truly connected
        esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, [](void* aArgs, esp_event_base_t aBase, int32_t aID, void* aData) {
            esp_netif_dns_info_t* theDnsInfo = &(SystemSetup::getInstance().myDnsInfo);
            theDnsInfo->ip.u_addr.ip4.addr = ipaddr_addr(DNS_SERVER);
            theDnsInfo->ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(SystemSetup::getInstance().myWifiHandle, ESP_NETIF_DNS_MAIN, theDnsInfo);
            xEventGroupSetBits(SystemSetup::getInstance().myConnectionEventGroup, SystemSetup::getInstance().myConnectionBit);
        }, nullptr, &myIPEventHandlerInstance);

        setClientConfig(aNetworkName, aNetworkPassword);  
        beginWifi(WifiMode::STA);
    } 

    void initWifiServer(const char* aNetworkName, const char* aNetworkPassword){ 
        if(!(myNetifIsReady && myEventLoopIsReady && myNVSIsReady)) return;
        initWifi(WifiMode::AP);
        setServerConfig(aNetworkName, aNetworkPassword);
        beginWifi(WifiMode::AP);
    } 

    static bool waitForWifiToConnect(uint32_t aConnectionTimeout = CONNECTION_TIMEOUT_MS){ 
        EventBits_t theConnectionBit =SystemSetup::getInstance().myConnectionBit;
        EventBits_t theConnectionResult = xEventGroupWaitBits(SystemSetup::getInstance().myConnectionEventGroup, theConnectionBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(aConnectionTimeout));
        return (theConnectionBit & theConnectionResult )!=0;
    } 

    void deinitNVS(){ 
        if(!myNVSIsReady)return; 
        nvs_flash_deinit(); 
        myNVSIsReady=false; 
    } 

    void deinitNetif(){ 
        if(!myNetifIsReady)return;
        esp_netif_deinit(); 
        myNetifIsReady = false; 
    }

    void deinitEventLoop(){ 
        if(!myEventLoopIsReady) return; 
        esp_event_loop_delete_default(); 
        myEventLoopIsReady=false;
    }

    void deInitWifiClient(){ 
        if(myWifiMode!=WifiMode::STA) return; 
        vEventGroupDelete(myConnectionEventGroup); 
        myConnectionEventGroup=nullptr;
        esp_wifi_disconnect();
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, myWifiEventHandlerInstance);
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, myIPEventHandlerInstance);
        endWifi(); 
    }

    void deInitWifiServer(){  
        if(myWifiMode!=WifiMode::AP) return;
        memset(&myDnsInfo, 0, sizeof(myDnsInfo)); 
        endWifi(); 
    } 
};