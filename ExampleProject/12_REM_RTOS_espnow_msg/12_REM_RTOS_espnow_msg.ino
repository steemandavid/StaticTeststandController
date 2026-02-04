// Static test stand controller
// 2024 David Steeman
// 20241229 RTOS, state machine, ADS1115, ringbuffer
// 20241230 SD Card write
// 20250102 Split code into remote and base functionality
// 20250102 OLED display
// 20250102 I/O's
// 20250115 Button debounce code

// I²C
#include <Wire.h>

// ESPNOW
#include <esp_now.h>
#include <WiFi.h>

// SSD1306 OLED display
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif12pt7b.h>

// ESPNOW
// Remote control MAC Address: 10:97:bd:cc:ed:bc
// Static test stand MAC Address: e4:65:b8:25:8a:a0
// Devboard MAC Address: e4:65:b8:25:8a:a0

// REPLACE WITH THE MAC Address of your receiver 
//uint8_t broadcastAddress[] = {0x10, 0x97, 0xBD, 0xCC, 0xED, 0xBC}; // COM3 Remote control MAC Address: 10:97:bd:cc:ed:bc
uint8_t broadcastAddress[] = {0xE4, 0x65, 0xB8, 0x25, 0x8A, 0xA0}; // COM7 Devboard MAC Address: e4:65:b8:25:8a:a0
esp_now_peer_info_t peerInfo = {};

// SSD1306 OLED display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// I/O defines
#define LED1 15
#define LED2 2
#define LED3 4
#define BUTTON_BUTTON 16
#define LED_BUTTON 2
#define SWITCH_ARM 27
#define LED_BUILTIN 32
#define BUZZER 25
#define VOLT_BAT 33 

#define I2C_SDA 4 // standard ESP32 is 21
#define I2C_SCL 5 // standard ESP32 is 22

// State machine (base)
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
#define WelcomeScreen      10
const String StateText[30] = {"Init", "Idle", "Armed", "Ignition", "StartTest", "TestRunning", "EndTest", "CalibrateLoadcell", "CalibratePressure", "CheckBreakwires", "WelcomeScreen"};
uint8_t BaseState = INIT; //set start State 

#define Booting 990
#define Running 991
uint8_t LocalState = Booting;

// Ringbuffer
#define RINGBUFFERSIZE 1000     //size of ring buffer to store sample data
#define SAMPLESPEED 100   // sample speed in milliseconds
volatile uint16_t BufferIndexIn = 0;
volatile uint16_t BufferIndexOut = 0;
volatile uint16_t BufferDrops = 0;

//typedef struct __attribute__((packed)) MsgStruct { // __attribute__((packed)) tells the compiler to avoid inserting padding for memory alignment, so the data is transmitted and received byte-for-byte.
#pragma pack(push, 1) // explicitly enforce no padding to avoid alignment or padding issues that could cause corruption when sending the data over ESPNOW
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


MsgStruct MessageReceived, MessageReceivedPrev, MessageToSend;
bool bNewMessageReceived = false;
//uint32_t MsgSent = 0, MsgRec = 0, MsgSentFailed = 0, MsgSentSuccess = 0; 

//char sDataRead[200] = "";

// RTOS task handles
TaskHandle_t xHandleMainLoop = NULL;
TaskHandle_t xHandleUpdateDisplay = NULL;
TaskHandle_t xHandleIOhandler = NULL;
TaskHandle_t xHandleButton_handler = NULL;

struct UpdateDisplayParamsStruct {
  bool UpdateDisplay;
  String BaseState;
  uint16_t TxRxFails;
  String Line[5] = {"", "", "", "", ""};
};

UpdateDisplayParamsStruct DisplayParamsCurrent = {false, "", 0, "", "", "", "", ""};
UpdateDisplayParamsStruct DisplayParamsNew     = {false, "", 0, "", "", "", "", ""};

bool LED_BUTTON_on = false;
bool LED_BUILTIN_on = false;
bool BUZZER_on = false;
//bool VOLT_BAT
bool bLedPinState = LOW;

// button debounce
#define TICK_PERIOD 5 // ms
#define NUM_KEYS 2
#define ANTI_DEBOUNCE_TIME  ( 100 / TICK_PERIOD )

uint8_t keys_array[ NUM_KEYS ] = { BUTTON_BUTTON, SWITCH_ARM };



// button debounce code - http://fjrg76.com/2021/12/30/how-to-decode-a-linear-keypad-with-arduino-and-c/
class Keypad {
  public:
    enum eKeys{ KEY_BUTTON, KEY_ARM, NO_KEY };
  private:
    uint8_t* keys{ nullptr };
    uint8_t  num_keys{ 0 };
    uint8_t  key{ 0 };
    bool     ready{ false };
  public:
    Keypad();
    // copies are not allowed:
    Keypad( Keypad& ) = delete;
    Keypad& operator=( Keypad& ) = delete;
    void init( uint8_t* keys, uint8_t num_keys );
    void state_machine();
    uint8_t read();
  private:
    uint8_t read_array();
};

Keypad::Keypad() {
  // nothing; all initialisations were carried out in the class body (as C++11 already allows it)
}

void Keypad::init( uint8_t* keys, uint8_t num_keys ) {
  Serial.println("Keypad.init() started.");
  this->keys = keys;
  this->num_keys = num_keys;
  for( uint8_t i = 0; i < this->num_keys; ++i ) {
    pinMode( keys[ i ], INPUT_PULLUP );
  }
  Serial.println("Keypad.init() completed.");
}

uint8_t Keypad::read_array() {
  uint8_t cont;
  for( cont = 0; cont < this->num_keys; ++cont ) {
    if( digitalRead( this->keys[ cont ] ) == LOW ) {
      break;
    }
  }
  return cont;
}

void Keypad::state_machine() {
  static uint8_t state = 0;
  static uint16_t ticks = 0;
  switch( state ) {
    case 0:
      this->key = read_array();
      if( this->key < this->num_keys ) {
        ticks = ANTI_DEBOUNCE_TIME;
        this->ready = false;
        state = 1;
      }
      break;
    case 1:
      --ticks;
      if( ticks == 0 ) {
        if( digitalRead( this->keys[ this->key ] ) == LOW ) {
          this->ready = true;
          state = 2;
        }
        else { // noise:
          state = 0;
        }
      }
      break;
    case 2:
        if( digitalRead( this->keys[ this->key ] ) == HIGH ) {
          ticks = ANTI_DEBOUNCE_TIME;
          state = 3;
        }
        break;
    case 3:
        --ticks;
        if( ticks == 0 ) {
          state = 0;
        }
        break;
    default:
      state = 0;
      ticks = 0;
      this->ready = false;
      break;
   }
}

uint8_t Keypad::read() {
  uint8_t ret_val = eKeys::NO_KEY;
  if( this->ready ) {
    this->ready = false;
    ret_val = this->key;
  }
  return ret_val;
}

Keypad keypad;
// Create the Keypad object



// RTOS tasks ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateDisplay(void *pvParameters) {
  Serial.println("Task UpdateDisplay started.");
  while(1) {
    if( strcmp(DisplayParamsCurrent.BaseState.c_str(), DisplayParamsNew.BaseState.c_str()) != 0 ||
        DisplayParamsCurrent.TxRxFails != DisplayParamsNew.TxRxFails ||
        strcmp(DisplayParamsCurrent.Line[0].c_str(), DisplayParamsNew.Line[0].c_str()) != 0 ||
        strcmp(DisplayParamsCurrent.Line[1].c_str(), DisplayParamsNew.Line[1].c_str()) != 0 ||
        strcmp(DisplayParamsCurrent.Line[2].c_str(), DisplayParamsNew.Line[2].c_str()) != 0 ||
        strcmp(DisplayParamsCurrent.Line[3].c_str(), DisplayParamsNew.Line[3].c_str()) != 0 ||
        strcmp(DisplayParamsCurrent.Line[4].c_str(), DisplayParamsNew.Line[4].c_str()) != 0       
      ) {  // If one of the values changed
      DisplayParamsCurrent = DisplayParamsNew; // Copy new values into struct to be displayed 
      display.clearDisplay();
      display.drawLine(0, 16, SCREEN_WIDTH, 16, WHITE);  // Horizontal line at the top of the blue area of the screen 
      display.drawLine(0, 17, SCREEN_WIDTH, 17, WHITE);  // Horizontal line at the top of the blue area of the screen 
      display.setTextSize(2); // set the font size, supports sizes from 1 to 8
      display.setCursor(0, 0);
      display.print(DisplayParamsCurrent.BaseState);
      if (DisplayParamsCurrent.TxRxFails<=9999) {
        display.setCursor(SCREEN_WIDTH-((DisplayParamsCurrent.TxRxFails==0?1:(int)log10(DisplayParamsCurrent.TxRxFails)+1)*12)-12, 0); // Align to the right of the screen
        display.print(DisplayParamsCurrent.TxRxFails);
      } else {
        DisplayParamsCurrent.TxRxFails=0; // reset counter
      }
//      display.print("o");
      display.setTextSize(1); // set the font size, supports sizes from 1 to 8
      for (int i=0; i<5; i++) { // Display line 2 through 6
        display.setCursor(0, 11+9*(i+1)); // put cursor on the correct y position for the respective line
        display.print(DisplayParamsCurrent.Line[i]); // print text
      }
      display.display();
    }
//    DisplayParamsNew.UpdateDisplay = false; // stop display from updating
    vTaskDelay( 100 / portTICK_PERIOD_MS );    
  }
}

void DisplayAddLine(String sLine) {
  vTaskSuspend(xHandleUpdateDisplay); // Prevent task from interfering with the buffer while it's being updated
  for (uint8_t i=0; i<5; i++) { // Move all lines up 1 line
    DisplayParamsNew.Line[i] = DisplayParamsNew.Line[i+1];
  }
  DisplayParamsNew.Line[4] = sLine; // Add new value to last line
//  DisplayParamsNew.UpdateDisplay = true;  // flag for displaying
  vTaskResume(xHandleUpdateDisplay);
}


uint8_t getKeyAction() {
  uint8_t pressed_key = keypad.read();
  if (pressed_key != Keypad::eKeys::NO_KEY) {
    switch (pressed_key) {
      case Keypad::eKeys::KEY_BUTTON:
//        Serial.println("getKeyAction() returned BUTTON_BUTTON");
        return BUTTON_BUTTON;
      case Keypad::eKeys::KEY_ARM:
//        Serial.println("getKeyAction() returned SWITCH_ARM");
        return SWITCH_ARM;
      default:
//         Serial.println("getKeyAction() returned NO_KEY");
       // If the key is neither KEY_BUTTON nor KEY_ARM, return NO_KEY
        return Keypad::eKeys::NO_KEY;
    }
  }
  return Keypad::eKeys::NO_KEY;
}


void ClearContents(struct MsgStruct *p) { // Reset variables which we're not sending to 0
  p->BaseState = 0;
  p->TxRxFails = 0;
  p->VbatRemote = 0;
  p->Button_Button = 0;
  p->Switch_Arm = 0;
  p->Button_LED = 0;
  p->Buzzer = 0;
  strcpy(p->DisplayLine, ""); 
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ESPNOW code from https://randomnerdtutorials.com/esp-now-two-way-communication-esp32/ //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send message via ESP-NOW
void EspNowSend(struct MsgStruct *MessageToSend) {
  printf("Sent: %i -\n", MessageToSend->BaseState);
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) MessageToSend, sizeof(*MessageToSend));
  delay(10);
  //  MsgSent++;
//  printf("Sent: %ld %ld %ld %ld\n", MsgSent, MsgRec, MsgSentSuccess, MsgSentFailed);
//  printf("Sent: %ld %i %ld %ld %ld %ld %i %i %i\n", MessageToSend->Time, MessageToSend->Command, MessageToSend->Thrust, MessageToSend->Pressure, MessageToSend->Igniter, MessageToSend->BreakWires, MessageToSend->TempAmbient, MessageToSend->TempCasing, MessageToSend->TempThroat);
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    DisplayParamsNew.TxRxFails = DisplayParamsCurrent.TxRxFails++; // update TxRxFails
    DisplayParamsNew.TxRxFails = 99999; // xxx debug. Value will get automatically overwritten by received packet so briefly show 99999
  } 
}

// Callback when data is received over ESPnow
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  Serial.println("\nReceived raw data:");
  for (int i = 0; i < sizeof(MsgStruct); i++) {
      Serial.printf("%02X ", ((uint8_t *) &incomingData)[i]);
  }
  Serial.println("\n");

  Serial.printf("\n----\nReceived struct size: %d\n", sizeof(MessageReceived));
  Serial.print("Bytes received: ");
  Serial.println(len);
  memcpy(&MessageReceived, incomingData, sizeof(MessageReceived));
//  MsgRec++;
//  printf("Recd: %ld %ld %ld %ld\n", MsgSent, MsgRec, MsgSentSuccess, MsgSentFailed);
  printf("Recv (OnDataRecv): %i %i %i %i %i %i --\n", 
         MessageReceived.BaseState, MessageReceived.VbatRemote, 
         MessageReceived.Button_Button, MessageReceived.Switch_Arm,
         MessageReceived.Button_LED, MessageReceived.Buzzer);
//  printf("Recd (OnDataRecv): %i %ld %i %i %i %i %s\n", MessageReceived.BaseState, MessageReceived.VbatRemote, MessageReceived.Button_Button, MessageReceived.Switch_Arm, MessageReceived.Button_LED, MessageReceived.Buzzer, MessageReceived.DisplayLine);
  bNewMessageReceived = true; // set flag to signal that new message was received to local state machine
  Serial.println("Msg received");
  if (bLedPinState == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
    bLedPinState = HIGH;
  } else {
    digitalWrite(LED_BUILTIN, LOW);
    bLedPinState = LOW;
  }
}

/*
//xxxdebug
// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Char: ");
  Serial.println(myData.a);
  Serial.print("Int: ");
  Serial.println(myData.b);
  Serial.print("Float: ");
  Serial.println(myData.c);
  Serial.print("Bool: ");
  Serial.println(myData.d);
  Serial.println();
}
*/

void Button_handler(void *pvParameters) {
  Serial.println("Task Button_handler started.");
  TickType_t xLastWakeTime;
  static unsigned long prev = 0;

  // Initialize xLastWakeTime with the current tick count
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    auto now = millis();
    if ( now - prev >= ( TICK_PERIOD ) ) {
      prev = now;
      keypad.state_machine();
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(ANTI_DEBOUNCE_TIME));
  }
}

void IOhandler(void *pvParameters) {
  Serial.println("Task IOhandler started.");
  while(1) {
/*
  total -= readings[readIndex];
  int sensorValue = analogRead(VOLT_BAT);
  float voltage = 0.001336 * sensorValue +1.3625; // linear regression
  readings[readIndex] = voltage;
  total += voltage;
  readIndex++;
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  volt_avg = total / numReadings;
  Serial.print("ADC: ");
  Serial.print(sensorValue);
  Serial.print(", Avg: ");
  Serial.println(volt_avg);
*/
    vTaskDelay( 100 / portTICK_PERIOD_MS );   //update IOs every 4 ms * 8 bits = 32ms for reading a button
  }
}

void MainLoop(void *parameter) {
  Serial.println("Task MainLoop started.");
  DisplayParamsNew.BaseState = "Booting";
  DisplayAddLine("State->Booting...");  
  Serial.println("State: Booting...");
  // Initialize message struct variables
  ClearContents(&MessageToSend);
  ClearContents(&MessageReceived);
  ClearContents(&MessageReceivedPrev);
  // Resume suspended tasks
//  vTaskResume(xHandleUpdateDisplay);
//  vTaskResume(xHandleIOhandler);
//  vTaskResume(xHandleButton_handler); 
  Serial.println("MainLoop: vTaskResume completed.");

  while(1) {
    if (bNewMessageReceived) { // new message received from base
      bNewMessageReceived = false; // reset flag
      Serial.println("MainLoop: New message received");
      printf("Recd (MainLoop): %i %ld %i %i %i %i %i %s\n", MessageReceived.BaseState, MessageReceived.TxRxFails, MessageReceived.VbatRemote, MessageReceived.Button_Button, MessageReceived.Switch_Arm, MessageReceived.Button_LED, MessageReceived.Buzzer, MessageReceived.DisplayLine);
      if (MessageReceived.BaseState != MessageReceivedPrev.BaseState) { // if new state received
        DisplayParamsNew.BaseState = MessageReceived.BaseState; // make new state available for display update function
      }
      DisplayParamsNew.TxRxFails = MessageReceived.TxRxFails; // update txrxfails
      if (strcmp(MessageReceived.DisplayLine, MessageReceivedPrev.DisplayLine) != 0) { // If new display line recieved
        DisplayAddLine(MessageReceived.DisplayLine); // Add line to display
      }
      if (MessageReceived.Button_LED != MessageReceivedPrev.Button_LED) { // if new state received
        digitalWrite(LED_BUTTON, MessageReceived.Button_LED); // set output
      }
      if (MessageReceived.Buzzer != MessageReceivedPrev.Buzzer) { // if new state received
//        digitalWrite(BUZZER, MessageReceived.Buzzer); // set output
      }
      MessageReceivedPrev = MessageReceived;
    }
    // Handle local inputs
    if (getKeyAction() == BUTTON_BUTTON) { // Button was pressed
      ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
      MessageToSend.Button_Button = true; // Set variable which we're sending
      EspNowSend(&MessageToSend); // Send keypress to base
    }
    if (getKeyAction() == SWITCH_ARM) { // Button was pressed
      ClearContents(&MessageToSend); // Reset variables which we're not sending to 0
      MessageToSend.Switch_Arm = true; // Set variable which we're sending
      EspNowSend(&MessageToSend); // Send keypress to base
    }
    vTaskDelay( 100 / portTICK_PERIOD_MS );   //xxx debug
  }
}



// setup ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup(void)
{
  Serial.begin(115200);
  
//  Serial.println(sizeof(MsgStruct));

  // Set up IO pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_BUTTON, INPUT_PULLUP);
  pinMode(LED_BUTTON, OUTPUT);
  pinMode(SWITCH_ARM, INPUT);
  pinMode(VOLT_BAT, INPUT);
  pinMode(BUZZER, OUTPUT);

  // Set up I²C
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin(); // Start I²C bus

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Setup(): SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
    // Add error handling here xxx todo
  } else {
    Serial.println(F("Setup(): SSD1306 allocation succesfull"));
  }
  vTaskDelay( 100 / portTICK_PERIOD_MS ); // give the OLED some time to initialize
  display.setFont(); // Use default display font
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2); // set the font size, supports sizes from 1 to 8
  vTaskDelay( 100 / portTICK_PERIOD_MS ); // give the OLED some time to initialize

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

  keypad.init( keys_array, NUM_KEYS ); // init button read code

  //esp_task_wdt_reset(); //xxx debug

// Function name of the task, Name of the task, Stack size (bytes), Parameter to pass, Task priority, Task handle, pin to a specific Core
  BaseType_t xReturned;
  xReturned = xTaskCreatePinnedToCore(UpdateDisplay, "UpdateDisplay", 4096, NULL, 1, &xHandleUpdateDisplay, tskNO_AFFINITY);
  if (xReturned != pdPASS) Serial.println(F("Setup(): Error: UpdateDisplay task creation failed."));
  Serial.println("Passed xTaskCreatePinnedToCore(UpdateDisplay");
  Serial.print("Free heap after creating Task: ");
  Serial.println(xPortGetFreeHeapSize());
  
  xReturned = xTaskCreatePinnedToCore(Button_handler, "Button_handler", 4096, NULL, 5, &xHandleButton_handler, tskNO_AFFINITY);
  if (xReturned != pdPASS) Serial.println(F("Setup(): Error: Button_handler task creation failed."));
  Serial.println("Passed xTaskCreatePinnedToCore(Button_handler");
  Serial.print("Free heap after creating Task: ");
  Serial.println(xPortGetFreeHeapSize());

  xReturned = xTaskCreatePinnedToCore(IOhandler, "IOhandler", 4096, NULL, 2, &xHandleIOhandler, tskNO_AFFINITY);
  if (xReturned != pdPASS) Serial.println(F("Setup(): Error: IOhandler task creation failed."));
  Serial.println("Passed xTaskCreatePinnedToCore(IOhandler");
  Serial.print("Free heap after creating Task: ");
  Serial.println(xPortGetFreeHeapSize());

  xReturned = xTaskCreatePinnedToCore(MainLoop, "MainLoop", 4096, NULL, 10, &xHandleMainLoop, tskNO_AFFINITY);
  if (xReturned != pdPASS) Serial.println(F("Setup(): Error: MainLoop task creation failed."));
  Serial.println("Passed xTaskCreatePinnedToCore(MainLoop");
  Serial.print("Free heap after creating Task: ");
  Serial.println(xPortGetFreeHeapSize());

//  vTaskSuspend(xHandleUpdateDisplay);
//  vTaskSuspend(xHandleButton_handler);
//  vTaskSuspend(xHandleIOhandler);
//  vTaskSuspend(xHandleMainLoop);
  Serial.println("Setup(): vTaskSuspend completed.");
  //Serial.println("Setup(): Starting scheduler.");
  //vTaskStartScheduler();
  //Serial.println("Setup(): Started scheduler.");

  Serial.println(__FILE__);
  Serial.println("\nSetup(): Setup complete\n");

//  vTaskDelay( 1000 / portTICK_PERIOD_MS );   //xxx debug
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main loop ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(void)
{
//  Serial.println("*");
//  delay(1000);

}


