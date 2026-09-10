//"Try to allocate memories of WiFi and LWIP in SPIRAM firstly. If failed, allocate internal memory" 
// ^^ SET THIS TO TRUE!!!! IDK WHY IT WOULD TRY TO ALLOCATE THIS IN INTERNAL MEORY ANYWAYS!!!

#include "header_files/SystemSetup.hpp" 
#include "header_files/Audio/InitAudioTask.hpp"
#include "header_files/Video/InitVideoTask.hpp"
#include "header_files/Controller/InitControllerTask.hpp"
#include "header_files/Settings/NVSAccess.hpp"
#include "header_files/Settings/HttpSettingsServer.hpp"

/*
#define URI "www.pc2gconline.party"
#define NETWORK_NAME "MyATT_035" 
#define NETWORK_PASSWORD "islandremedy015"
*/

#define SERVER_NAME "Settings"
#define SERVER_PASSWORD "12345678"

#define MATRIX_ROWS 5
#define MATRIX_COLUMNS 5  

#define SERVER_STACK_SIZE 8192

#define NETWORK_NAME_KEY "WifiName" 
#define NETWORK_NAME_PASSWORD_KEY "WifiPassword" 
#define HOST_NAME_KEY "HostName"  
#define PAGE_LOCK_KEY "PageLock"


static size_t isIndex1Or0(const char* aPageLockStr, size_t aRow, size_t aCol) { return aCol == (aPageLockStr[aRow] - '0' - 1) ? 1:0; } 

static cJSON* createRow(const char* aPageLockStr, size_t aRow){
    cJSON *theRow = cJSON_CreateArray();
    for (size_t col = 0; col < MATRIX_COLUMNS; col++) cJSON_AddItemToArray(theRow, cJSON_CreateNumber(isIndex1Or0(aPageLockStr,aRow, col)));                
    return theRow;
}

static char* initArray(const char* aPageLockStr, cJSON* aRoot){ 
    cJSON *theMatrix = cJSON_CreateArray();
    for (size_t row = 0; row < MATRIX_ROWS; row++) cJSON_AddItemToArray(theMatrix, createRow(aPageLockStr, row));
    cJSON_AddItemToObject(aRoot, "lockmatrix", theMatrix); 
    return cJSON_PrintUnformatted(aRoot); 
} 

extern "C" void app_main(void){ 
  SystemSetup::getInstance().initNVS();
  SystemSetup::getInstance().initEventLoop();
  SystemSetup::getInstance().initNetif();
  SystemLED::getInstance().turnOff();
  
  gpio_set_direction(GPIO_NUM_9, GPIO_MODE_INPUT);

  if(gpio_get_level(GPIO_NUM_9)){ 
      SystemSetup::getInstance().initWifiServer(SERVER_NAME, SERVER_PASSWORD); 
      SystemLED::getInstance().writeColor(255,255,255); 

      static HttpSettingsServer theServer; 
      theServer 
      .buildSettingsForm({NETWORK_NAME_KEY, NETWORK_NAME_PASSWORD_KEY, HOST_NAME_KEY, PAGE_LOCK_KEY}) 
      .buildPostCallback([](JSONObject& aObject){ 
          cJSON *thePageLockRoot = cJSON_CreateObject();  
          char* theRealPageLock = initArray(aObject[PAGE_LOCK_KEY].as<char*>(), thePageLockRoot);  
          
          NVSAccess::getInstance().openNVSAccess();
          NVSAccess::getInstance().setStr(NETWORK_NAME_KEY, aObject[NETWORK_NAME_KEY].as<char*>());
          NVSAccess::getInstance().setStr(NETWORK_NAME_PASSWORD_KEY, aObject[NETWORK_NAME_PASSWORD_KEY].as<char*>());
          NVSAccess::getInstance().setStr(HOST_NAME_KEY, aObject[HOST_NAME_KEY].as<char*>());
          NVSAccess::getInstance().setStr(PAGE_LOCK_KEY, theRealPageLock);
          NVSAccess::getInstance().closeNVSAccess();

          cJSON_free(theRealPageLock);
          cJSON_Delete(thePageLockRoot);}) 
      .buildEsp32ResetsUponSubmission()
      .buildInit(); 

      theServer.initWebServer(SERVER_STACK_SIZE);
      return;
  }

  size_t theNetNameSize=32; // need to be initialized to a number, get str uses this as a 
  size_t theNetPassWordSize=32; 
  size_t theHostNameSize=32; 
  size_t thePageLockSize=80;

  char theNetName[32]={};
  char theNetPassword[32]={};
  char theHostName[32]={};
  char thePageLock[80]={};

  NVSAccess::getInstance().openNVSAccess();
  NVSAccess::getInstance().getStr(NETWORK_NAME_KEY, theNetName, &theNetNameSize);
  NVSAccess::getInstance().getStr(NETWORK_NAME_PASSWORD_KEY, theNetPassword, &theNetPassWordSize);
  NVSAccess::getInstance().getStr(HOST_NAME_KEY, theHostName, &theHostNameSize);
  NVSAccess::getInstance().getStr(PAGE_LOCK_KEY, thePageLock, &thePageLockSize);  
  NVSAccess::getInstance().closeNVSAccess();

  printf("Network Name: ");
  printf(theNetName); 
  printf("\n");  
  printf("Network Password: ");
  printf(theNetPassword); 
  printf("\n");  
  printf("Host Name: ");
  printf(theHostName); 
  printf("\n");  
  printf("Page Lock: ");
  printf(thePageLock); 
  printf("\n"); 

  SystemSetup::getInstance().initWifiClient(theNetName, theNetPassword); 
  InitVideoTask::initVideoTask<6, 1, 4096>(theHostName);
  InitAudioTask::initAudioTask<7, 0, 4096>(theHostName); 
  InitControllerTask::initControllerTask<3, 0, 4096>(theHostName); 

  // make sure video quality is < 20, otherwise there will be a buildup of frames in the rx buffer of the wi fi chip 
  // separate the backend buffer size of the websocket with the buffer used to swap into the circular buffer 
  // chunk screen writes so as not to overload dma 

}
