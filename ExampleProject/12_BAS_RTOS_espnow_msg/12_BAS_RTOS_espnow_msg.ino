// Static test stand controller
// 2024 David Steeman
// 20241229 RTOS, state machine, ADS1115, ringbuffer
// 20241230 SD Card write

// ADS1115 ADC
#include <Streaming.h>   //  from "Streaming by Mikal Hart" in library manager
#include <ADS1115_WE.h> 
#include <Wire.h>

// SD card
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// ESPNOW
#include <esp_now.h>
#include <WiFi.h>

// ESPNOW
// Remote control MAC Address: 10:97:bd:cc:ed:bc
// Static test stand MAC Address: e4:65:b8:25:8a:a0
// Devboard MAC Address: e4:65:b8:25:8a:a0

// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddress[] = {0x10, 0x97, 0xBD, 0xCC, 0xED, 0xBC}; // COM3 Remote control MAC Address: 10:97:bd:cc:ed:bc
//uint8_t broadcastAddress[] = {0xE4, 0x65, 0xB8, 0x25, 0x8A, 0xA0}; // COM7 Devboard MAC Address: e4:65:b8:25:8a:a0
esp_now_peer_info_t peerInfo = {};

// ADS1115
volatile bool convReady = false;
ADS1115_WE adc = ADS1115_WE(0x48);
const uint8_t interruptPin = 16;  // pin2 is default SDA 

// Interrupt service routine for external interrupt. Make sure it is in RAM
void IRAM_ATTR convReadyAlert(){
   convReady = true;
}

// I/O defines
#define LED1 15
#define LED2 2
#define LED3 4
#define SD_CS 5
#define SD_MOSI 23
#define SD_CLK 18
#define SD_MISO 19
#define LED_BUILTIN 2 // for ESP32 devboard

// State machine
#define INIT               0
#define IDLE               1
#define ARMED              2
#define IGNITION           3
#define STARTTEST          4
#define TESTRUNNING        5
#define ENDTEST            6
#define CalibrateLoadcell  7
#define CalibratePressure  8
#define CheckBreakwires    9
#define SetSampleSpeed     10
#define SetUnits           11
#define WriteConfig        12
#define DisplayConfig      13
#define ForceTestStart     14
#define SetActiveChannels  15
#define SetIdentification  16
#define DisplayHelp        17
#define WelcomeScreen      18
const String StateText[30] = {"Init", "Idle", "Armed", "Ignition", "StartTest", "TestRunning", "EndTest", "CalibrateLoadcell", "CalibratePressure", "CheckBreakwires", "WelcomeScreen"};
uint8_t State = INIT; //set start State 

// Ringbuffer
#define RINGBUFFERSIZE 1000  //size of ring buffer to store sample data
#define SAMPLESPEED 100   // sample speed in milliseconds
volatile uint16_t BufferIndexIn = 0;
volatile uint16_t BufferIndexOut = 0;
volatile uint16_t BufferDrops = 0;

typedef struct DataStruct {
  uint16_t Thrust;
  uint16_t Pressure;
  uint16_t Igniter;
  uint16_t BreakWires;
  uint8_t  TempAmbient;
  uint8_t  TempCasing;
  uint8_t  TempThroat;  
} DataStruct;

//typedef struct __attribute__((packed)) MsgStruct { // __attribute__((packed)) tells the compiler to avoid inserting padding for memory alignment, so the data is transmitted and received byte-for-byte.
#pragma pack(push, 1)
typedef struct MsgStruct {
  uint8_t BaseState;
  uint16_t TxRxFails;
  uint8_t VbatRemote;
  bool Button_Button;
  bool Switch_Arm;
  bool Button_LED;
  bool Buzzer;
  char DisplayLine[10];
} MsgStruct;
#pragma pack(pop)


//xx debug
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;
struct_message myData;


DataStruct RingBuffer[RINGBUFFERSIZE];

MsgStruct MessageReceived, MessageReceivedPrev, MessageToSend;
bool bNewMessageReceived = false;

bool bLedPinState = LOW;

uint16_t TxRxFails = 0;

String sFilename = "/test";

char sDataRead[200] = "";

// RTOS task handles
TaskHandle_t xHandleGetSample = NULL;
TaskHandle_t xHandleStateMachine = NULL;
TaskHandle_t xHandleReadDataFromBuffer = NULL;


// RTOS tasks ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetSample(void *parameter) {
//  pinMode(LED1, OUTPUT);
  while(1) {
//    digitalWrite(LED1, LOW);
    if ( (BufferIndexIn + 1) % RINGBUFFERSIZE != BufferIndexOut) {                  //if buffer not full
//        printf("GetSample1 >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
//      adc.setCompareChannels(ADS1115_COMP_0_GND); // Set the active ADC input (there is only 1 result register which can be read)
      if(convReady) {
        convReady = false;  //reset ready indicator
        RingBuffer[BufferIndexIn].Thrust     = random(1, 100);
        RingBuffer[BufferIndexIn].Pressure   = random(101, 200);
//        RingBuffer[BufferIndexIn].Igniter    = adc.getResult_mV();
//        RingBuffer[BufferIndexIn].BreakWires = adc.getResult_mV();
//        printf("%ld %ld\n", RingBuffer[BufferIndexIn].Thrust, RingBuffer[BufferIndexIn].Pressure); // xxx debug
        BufferIndexIn = (BufferIndexIn + 1) % RINGBUFFERSIZE;                      //increase buffer pointer and wrap ringbuffer if necessary
      }
    } else {                                                                     //if buffer full
/*      if ( (State != StartTest) && (State != Ignition) && (State != TestRunning)) {                                               //if test has not started (probably Idle)
        printf("GetSample2 >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
        BufferIndexOut = (BufferIndexOut + 1) % RINGBUFFERSIZE;                   //overwrite oldest value to make room
      } else {                                                                  //if test is running
        printf("GetSample3 >%ld %ld> BUFFERDROP\n", BufferIndexIn, BufferIndexOut); // xxx debug
        BufferDrops++;                                                         //signal that sample was dropped
      }
*/    }
    vTaskDelay( SAMPLESPEED / portTICK_PERIOD_MS );
//    digitalWrite(LED1, HIGH);
  }
}

void ReadDataFromBuffer(void *parameter) {
    pinMode(LED2, OUTPUT);
    while(1) {
      digitalWrite(LED2, LOW);
      if (BufferIndexIn != BufferIndexOut) {                              //if buffer is not empty        
//        printf("ReadBuffer1 >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
        // Send to console
//        sprintf(sDataRead, "RDFB %ld %i %ld %ld\n", RingBuffer[BufferIndexOut].Time, RingBuffer[BufferIndexOut].Command, RingBuffer[BufferIndexOut].Thrust, RingBuffer[BufferIndexOut].Pressure);
        // Send over ESPNOW
//        EspNowSend(&RingBuffer[BufferIndexOut]);
        // Save to SD card
        appendFile(SD, sFilename.c_str(), sDataRead);
        BufferIndexOut = (BufferIndexOut + 1) % RINGBUFFERSIZE;           //increase buffer pointer and wrap ringbuffer if necessary
      } else { // buffer is empty
//        printf("%%\n"); // xxx debug
//        printf("ReadBuffer2 >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
      }
      digitalWrite(LED2, HIGH);
//      vTaskDelay( SAMPLESPEED/2 / portTICK_PERIOD_MS ); //xxx debug
    }
}

void StateMachine(void *parameter) {
  pinMode(LED3, OUTPUT);
  while(1) {
    digitalWrite(LED3, LOW);
    switch(State) {                                                           //State machine
      case INIT:                                                              //Initialization
        Serial.println("State: INIT");
        // Suspend tasks
        vTaskSuspend(xHandleGetSample);
        vTaskSuspend(xHandleReadDataFromBuffer);
        // RingBuffer init        
        BufferIndexIn = 0;
        BufferIndexOut = 0;
        BufferDrops = 0;
        // Initialize message struct variables
        ClearContents(&MessageToSend);
        ClearContents(&MessageReceived);
        ClearContents(&MessageReceivedPrev);

        // ESPNOW init
        Serial.print("Setup(): Broadcast Address of peer: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", broadcastAddress[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println("");
        WiFi.mode(WIFI_STA); // Set device as a Wi-Fi Station
        // Init ESP-NOW
        if (esp_now_init() != ESP_OK) {
          Serial.println("Setup(): esp_now_init() != ESP_OK");
        } else {
          Serial.println("Setup(): esp_now_init() = ESP_OK");
        }
        esp_now_register_send_cb(OnDataSent); // register Send callback function to get the status of Transmitted packet
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv)); // Register for a callback function that will be called when data is received
/*
wifi_config_t config;
esp_wifi_get_config(WIFI_IF_STA, &config);
config.sta.channel = 1; // Use channel 1 (or any other fixed channel)
esp_wifi_set_config(WIFI_IF_STA, &config);
*/
        // Register peer
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 1;  
        peerInfo.encrypt = false;
        // Add peer        
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
          Serial.println("Setup(): EspNow: Failed to add peer");
          // xxx todo: start buzzer + halt code
        } else {
          Serial.println("Setup(): EspNow: peer added succesfully");
        }


/*        // Initialize EspNow and catch errors
        if (!EspNowInit()) { 
          Serial.println("EspNow init error");
          // to do; handle EspNow error here
          // Beep buzzer
          // return to init state?
        } else {
          Serial.println("EspNowInit succeeded");
          // Send message to remote
          ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
          MessageToSend.BaseState = INIT; // Set variable which we're sending
          printf("MessageToSend.BaseState = %i (%s)\r\n", MessageToSend.BaseState, StateText[MessageToSend.BaseState]); //xxx debug
          EspNowSend(&MessageToSend); // Send to remote
          delay(500);
        }
*/
        //xxx debug
/*        strcpy(myData.a, "THIS IS A CHAR");
        myData.b = random(1,20);
        myData.c = 1.2;
        myData.d = false;
*/
        // Initialize SD card and catch errors
/*        if (!SDcardInit()) { 
          Serial.println("SD card init error");
          // handle errors here
          // Beep buzzer
          // Send to remote for displaying
          // return to init state?
        }
*/        // wait for response from remote to see if it is on. Perhaps put this in a different state?
        // Move to next state
        State = IDLE;
        break;
      case IDLE:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
//        MessageToSend.BaseState = IDLE; // Set variable which we're sending (as uint8)
        MessageToSend.BaseState = 2; // Set variable which we're sending (as text)
        printf("MessageToSend.BaseState = %i %s\r\n", MessageToSend.BaseState, StateText[MessageToSend.BaseState]); //xxx debug
//x        MessageToSend.TxRxFails = TxRxFails; // ++ = debug

/*        //xxx debug
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
        Serial.println("Sent myData");
*/
        Serial.println("\nSending raw data:");
        for (int i = 0; i < sizeof(MsgStruct); i++) {
            Serial.printf("%02X ", ((uint8_t *) &MessageToSend)[i]);
        }
        Serial.println("\n");

        EspNowSend(&MessageToSend); // Send to remote
        Serial.println("State: IDLE");
        // ping - pong
        // listen for incoming commands
        delay(1000); //xxx debug
//        State = ARMED;
        break;
      case ARMED:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
        MessageToSend.BaseState = ARMED; // Set variable which we're sending
        printf("MessageToSend.BaseState = %s\r\n", StateText[MessageToSend.BaseState]); //xxx debug
        EspNowSend(&MessageToSend); // Send to remote
        State = STARTTEST;
        // sound alarm
        break;
      case STARTTEST:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
        MessageToSend.BaseState = STARTTEST; // Set variable which we're sending
        printf("MessageToSend.BaseState = %s\r\n", StateText[MessageToSend.BaseState]); //xxx debug
        EspNowSend(&MessageToSend); // Send to remote
//       sprintf(sFilename, "/%ld.txt", esp_timer_get_time());
//        sFilename += "test54654.txt";
//        writeFile(SD, sFilename, "Start of test\n\n");
        // disable unneccessary tasks?
        // disable incoming commands?
        // write current contents of the ringbuffer to SD card
        // start logging new data from ringbuffer to SD card
        // start transmitting data
//          printf("SM StartTest >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
        vTaskResume(xHandleGetSample);
        vTaskResume(xHandleReadDataFromBuffer);
        State = IGNITION;
        break;
      case IGNITION:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
        MessageToSend.BaseState = IGNITION; // Set variable which we're sending
        printf("MessageToSend.BaseState = %s\r\n", StateText[MessageToSend.BaseState]); //xxx debug
         EspNowSend(&MessageToSend); // Send to remote
        State = TESTRUNNING;
        // stop alarm
        // enable ignition
        // send power to ignitor
        break;
      case TESTRUNNING:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
        MessageToSend.BaseState = TESTRUNNING; // Set variable which we're sending
        printf("MessageToSend.BaseState = %s\r\n", StateText[MessageToSend.BaseState]); //xxx debug
        EspNowSend(&MessageToSend); // Send to remote
        State = ENDTEST;
        break;
      case ENDTEST:
        // Send message to remote
        ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
        MessageToSend.BaseState = ENDTEST; // Set variable which we're sending
        printf("MessageToSend.BaseState = %s\r\n", StateText[MessageToSend.BaseState]); //xxx debug
        EspNowSend(&MessageToSend); // Send to remote
        // how to detect end of test?
        // stop logging data to SD card
        // stop transmitting data
        vTaskSuspend(xHandleGetSample);
        vTaskSuspend(xHandleReadDataFromBuffer);
          printf("SM EndTest >%ld %ld>\n", BufferIndexIn, BufferIndexOut); // xxx debug
        readFile(SD, sFilename.c_str());
        State = 666; //xxx debug
        break;
      default:
//        Serial.println("State: default");
        break;
    }
    digitalWrite(LED3, HIGH);
    vTaskDelay( 1000 / portTICK_PERIOD_MS );   //xxx debug
  }
}


// setup ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup(void)
{
  Serial.begin(115200);
 
  //Serial.println(sizeof(MsgStruct));

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  // Set up ADS1115 ADC
  Wire.begin();
  Wire.setClock(400000);
  pinMode(interruptPin, INPUT_PULLUP);
  if(!adc.init()){
    Serial.println("ADS1115 not connected!");
  }
  
  adc.setVoltageRange_mV(ADS1115_RANGE_4096); // Set the voltage range of the ADC to adjust the gain
  adc.setAlertPinMode(ADS1115_ASSERT_AFTER_1); //needed in this sketch to enable alert pin (doesn't matter if you choose after 1,2 or 4)
  adc.setConvRate(ADS1115_860_SPS); // Set the conversion rate in SPS (samples per second)
  adc.setMeasureMode(ADS1115_CONTINUOUS); // the conversion ready alert pin also works in continuous mode
  adc.setAlertPinToConversionReady(); //needed for this sketch
  attachInterrupt(digitalPinToInterrupt(interruptPin), convReadyAlert, FALLING);
  adc.setCompareChannels(ADS1115_COMP_0_GND); // Set the active ADC input (there is only 1 result register which can be read)

  xTaskCreatePinnedToCore(
    GetSample,      // Function name of the task
    "GetSample",   // Name of the task (e.g. for debugging)
    4086,        // Stack size (bytes)
    NULL,        // Parameter to pass
    5,           // Task priority
    &xHandleGetSample,        // Task handle
    1            // pin to a specific Core    
  );
  vTaskSuspend(xHandleGetSample);

  xTaskCreatePinnedToCore(
    StateMachine,     // Function name of the task
    "StateMachine",  // Name of the task (e.g. for debugging)
    4086,       // Stack size (bytes)
    NULL,       // Parameter to pass
    1,          // Task priority
    &xHandleStateMachine,        // Task handle
    1            // pin to a specific Core    
  );
//  vTaskSuspend(xHandleStateMachine);

  xTaskCreatePinnedToCore(
    ReadDataFromBuffer,     // Function name of the task
    "ReadDataFromBuffer",  // Name of the task (e.g. for debugging)
    4086,       // Stack size (bytes)
    NULL,       // Parameter to pass
    2,          // Task priority
    &xHandleReadDataFromBuffer,        // Task handle
    0            // pin to a specific Core    
  );
  vTaskSuspend(xHandleReadDataFromBuffer);

  while (!Serial) {
    delay(10);
  }

  Serial.println(__FILE__);
  Serial.println("\nSetup complete\n");

  vTaskDelay( 1000 / portTICK_PERIOD_MS );   //xxx debug
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main loop ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(void)
{
//  Serial.println("*");
//  delay(1000);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ESPNOW code from https://randomnerdtutorials.com/esp-now-two-way-communication-esp32/ //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Initialize ESPnow
bool EspNowInit(void) {
  // ESPNOW init
  WiFi.mode(WIFI_STA); // Set device as a Wi-Fi Station
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init() != ESP_OK");
    return 0;
  } else {
    Serial.println("esp_now_init() == ESP_OK");
  }
  esp_now_register_send_cb(OnDataSent); // register Send callback function to get the status of Trasnmitted packet
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // Add peer      
  esp_err_t result = esp_now_add_peer(&peerInfo);  
  printf("esp_now_add_peer: %s\r\n", esp_err_to_name(result));
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv)); // Register for a callback function that will be called when data is received
  return 1; // success
}

// Send message via ESP-NOW
void EspNowSend(struct MsgStruct *Msg) {
  printf("Sent: %i -\n", Msg->BaseState);
  Serial.printf("Sending struct size: %d\n", sizeof(MsgStruct));
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &Msg, sizeof(Msg)); // result only returns if message was added to the sent queue succesfully, NOT if it was effectively sent and received, so we ignore it
  delay(10);
//x  printf("Sent: %i %ld %i %i %i %i %i %s\n", Msg->BaseState, Msg->TxRxFails, Msg->VbatRemote, Msg->Button_Button, Msg->Switch_Arm, Msg->Button_LED, Msg->Buzzer, Msg->DisplayLine);
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    TxRxFails++; // update TxRxFails
    printf("OnDataSent failed, TxRxFails = %ld\r\n", TxRxFails);
//    TxRxFailsNew = 99999; // xxx debug. Value will get automatically overwritten by received packet so briefly show 99999
  } else { // Sent succesfully
    Serial.println("OnDataSent ESP_NOW_SEND_SUCCESS");
    if (bLedPinState == LOW) {
      digitalWrite(LED_BUILTIN, HIGH);
      bLedPinState = HIGH;
    } else {
      digitalWrite(LED_BUILTIN, LOW);
      bLedPinState = LOW;
    }
  }
}

// Callback when data is received
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&MessageReceived, incomingData, sizeof(MessageReceived));
  printf("Recv (OnDataRecv): %i %i %i %i %i %i --\n", 
         MessageReceived.BaseState, MessageReceived.VbatRemote, 
         MessageReceived.Button_Button, MessageReceived.Switch_Arm,
         MessageReceived.Button_LED, MessageReceived.Buzzer);
  //  MsgRec++;
//  printf("Recd: %ld %ld %ld %ld\n", MsgSent, MsgRec, MsgSentSuccess, MsgSentFailed);
//  printf("Recd (OnDataRecv): %i %ld %i %i %i %i %s\n", MessageReceived.BaseState, MessageReceived.VbatRemote, MessageReceived.Button_Button, MessageReceived.Switch_Arm, MessageReceived.Button_LED, MessageReceived.Buzzer, MessageReceived.DisplayLine);
  bNewMessageReceived = true; // set flag to signal that new message was received to local state machine
}




// SD card code from https://raw.githubusercontent.com/RuiSantosdotme/Random-Nerd-Tutorials/master/Projects/ESP32/MicroSD_Card/ESP32_SD_Test.ino ///////////////////////////////////////////////////////////

bool SDcardInit(void) {
  sFilename += String(random(0, 10000)); // create new filename
  sFilename += ".txt";
  Serial.print("Filename for storing test results: ");
  Serial.println(sFilename);
  writeFile(SD, sFilename.c_str(), "Start of test\n\n");
  if(!SD.begin(SD_CS)){
    Serial.println("SD Card Mount Failed");
    return 0;
  } else {
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return 0;
    }
    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
      Serial.println("MMC");
    } else if(cardType == CARD_SD){
      Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
    if ((SD.totalBytes() - SD.usedBytes()) < 1 * 1024 * 1024) {
      Serial.println("ERROR: not enough disk space!");
      return 0;
    }
  }
  listDir(SD, "/", 0); // xxx debug
  return 1; // success
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
//  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path){
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if(file){
    len = file.size();
    size_t flen = len;
    start = millis();
    while(len){
      size_t toRead = len;
      if(toRead > 512){
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for(i=0; i<2048; i++){
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}



void ClearContents(struct MsgStruct *p) { // Reset variables which we're not sending to 0
  p->BaseState = 0;
  p->TxRxFails = 0;
  p->VbatRemote = 0;
  p->Button_Button = 0;
  p->Switch_Arm = 0;
  p->Button_LED = 0;
  p->Buzzer = 0;
  strcpy(p->DisplayLine, "\0"); 
}
