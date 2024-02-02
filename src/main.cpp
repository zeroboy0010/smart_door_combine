#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <UniversalTelegramBot.h>  
#include <EEPROM.h>
#include <PN532_HSU.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include "esp_heap_caps.h"
// Initialize Telegram BOT
#define BOTtoken "6597802948:AAHiJvLsXpzgeKHNKCgJBLyR15Yk8hYkBNw"  // your Bot Token (Get from Botfather)

/* 1. Define the WiFi credentials */
char WIFI_SSID[20];
char WIFI_PASSWORD[20];
bool Wifi_status = true;
bool to_connect_wifi;
//freertos
QueueHandle_t UID_String;
QueueHandle_t Password_login;
SemaphoreHandle_t right_log;
SemaphoreHandle_t wrong_log;


// Define two tasks for Blink & AnalogRead.
void Task_read_RFID( void *pvParameters );
void Task_read_get_key_pad  ( void *pvParameters );
void Task_Data  ( void *pvParameters );
void Task_Run_AP  ( void *pvParameters );
void Main_task(void *pvParameters);
void Task_telegram_bot(void * pvParameters);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
// Define OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


//globaldata from eeprom
String Input;
String Output;
String Password;
String SSID;
String Tag_UID;
String Password_door;
String Password_Read;
String SSID_Read;
String Password_door_Read;
String Tag_UID_read;
String Telegram_ID_read;
String Telegram_ID;

int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);

int config_button = 15;
// connect wifi
void connect();
void running_ap();
void(* resetFunc) (void) = 0; //declare reset function @ address 0
void cardreading(){
  Serial.println("recieve");
}

unsigned long last_ttick;
uint8_t ttick;

void setup() {
  EEPROM.begin(300);
  Serial.begin(9600);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  Serial.println("LCD ready!!!");
  pinMode(config_button,INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(5), cardreading, FALLING);
  //global data

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 10);
  display.print("Start!!");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(2000));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 10);
  for (int i=0; i<30;i++){
    Serial.print(".");
    display.print(".");
    display.display();
    if (digitalRead(config_button) == 0){   // 
      to_connect_wifi = 0;
      Wifi_status = 0 ;
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(25, 10);
      display.print("Config mode!!");
      display.display();
      break;
    }
    else {
      to_connect_wifi = 1;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }


  if (Wifi_status == true && to_connect_wifi == 1){
      connect(); 
      
  }
  vTaskDelay(pdMS_TO_TICKS(2000));
  right_log = xSemaphoreCreateBinary();
  wrong_log = xSemaphoreCreateBinary();
  // put your setup code here, to run once:
  UID_String = xQueueCreate(3,sizeof(String));
  Password_login = xQueueCreate(8,sizeof(char));



  xTaskCreatePinnedToCore(
    Task_read_get_key_pad
    ,  "Task_read_get_key_pad" // A name just for humans
    ,  2048 * 4     // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  5  // Priority
    ,  NULL
    , 1 // Task handle is not used here - simply pass NULL
    );

  if (to_connect_wifi == 0 ){
    xTaskCreatePinnedToCore(
    Task_Run_AP
    ,  "Task_Run_AP" // A name just for humans
    ,  2048 * 7      // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  4  // Priority
    ,  NULL
    , 0 // Task handle is not used here - simply pass NULL
    );
  }
  else if (Wifi_status == 1){
    xTaskCreatePinnedToCore(
    Task_telegram_bot
    ,  "Task_telegram_bot" // A name just for humans
    ,  2048 * 4      // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  1 // Priority
    ,  NULL
    , 0 // Task handle is not used here - simply pass NULL
    );
    xTaskCreatePinnedToCore(
    Main_task
    ,  "Main_task" // A name just for humans
    ,  2048 * 6       // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  5  // Priority
    ,  NULL
    , 1 // Task handle is not used here - simply pass NULL
    );
  }

  xTaskCreatePinnedToCore(
    Task_read_RFID
    ,  "Task_read_RFID" // A name just for humans
    ,  2048 * 5      // The stack size can be checked by calling `uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);`
    ,  NULL // Task parameter which can modify the task behavior. This must be passed as pointer to void.
    ,  4  // Priority
    ,  NULL
    , 1 // Task handle is not used here - simply pass NULL
    );


}

void loop() {
  // put your main code here, to run repeatedly:
  while (millis() -  last_ttick >1000){
    last_ttick = millis();
    if (digitalRead(config_button) ==0){
      ttick++;
      if (ttick > 4){
        resetFunc();
      }
    }
    else{
      ttick = 0;
    }
  }
}

// put function definitions here:
void Task_read_RFID( void *pvParameters ){
  PN532_HSU pn532hsu(Serial2);
  NfcAdapter nfc = NfcAdapter(pn532hsu);
  nfc.begin();
  vTaskDelay(pdMS_TO_TICKS(1000));
  for(;;){
    if(nfc.tagPresent()){
      NfcTag tag = nfc.read();
      Serial.println(tag.getTagType());
      Serial.print("UID: ");Serial.println(tag.getUidString());
      String data_read = tag.getUidString();
      xQueueSend(UID_String, ( void * ) &data_read, 1);
      data_read.clear();
    }
    vTaskDelay(pdMS_TO_TICKS(1200));
  }
  // vTaskDelete(NULL);
}
void Task_read_get_key_pad  ( void *pvParameters ){

  // get_keypad
  // Define the get_keypad layout
  const byte ROW_NUM = 4; // four rows
  const byte COLUMN_NUM = 4; // four columns

  char get_keys[ROW_NUM][COLUMN_NUM] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
  };

  byte pin_rows[ROW_NUM]    =  {27,14,12,13};    // connect to the row pinouts of the get_keypad
  byte pin_column[COLUMN_NUM] = {32,33,25,26};   // connect to the column pinouts of the get_keypad
  Keypad keypad = Keypad(makeKeymap(get_keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
  for(;;){
    char get_key = keypad.getKey();
    if(get_key){
      xQueueSend(Password_login, ( void * ) &get_key, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void Main_task(void *pvParameters){
  //
  #define Selenoir_pin 2
  pinMode(Selenoir_pin ,OUTPUT);
  // display

  String inputCode = "";
  vTaskDelay(pdMS_TO_TICKS(500));
  if (Wifi_status == 1){
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(25, 10);
    display.print("working....");
    display.display();
  }
  vTaskDelay(pdMS_TO_TICKS(500));

  String UID_get;
  // pinMode(5,INPUT);
  for(;;){  
    // Serial.println(digitalRead(5));
    if(xQueueReceive(UID_String,&UID_get, 1) == pdTRUE){
      if(UID_get == Tag_UID_read){
        UID_get.clear();
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(25, 10);
        display.print("Correct!");
        display.display();
        xSemaphoreGive(right_log);
        digitalWrite(Selenoir_pin,1);
        vTaskDelay(pdMS_TO_TICKS(3000));
        digitalWrite(Selenoir_pin,0);
      }
      else {
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(20, 10);
        display.print("Invalid!");
        display.display();
        //
        xSemaphoreGive(wrong_log);
        //
        vTaskDelay(pdMS_TO_TICKS(1000));
        display.clearDisplay();
      }
    }

    char get_key;
    if(xQueueReceive(Password_login,&get_key, 1) == pdTRUE){
      if (get_key) {
        if (get_key == '#') {
          if (inputCode.length()  < 10) {
            if (inputCode == Password_door_Read) {
              display.clearDisplay();
              display.setTextSize(2);
              display.setTextColor(SSD1306_WHITE);
              display.setCursor(25, 10);
              display.print("Correct");
              display.display();
              //
              xSemaphoreGive(right_log);
              //
              digitalWrite(Selenoir_pin,1);
              vTaskDelay(pdMS_TO_TICKS(3000));
              digitalWrite(Selenoir_pin,0);
              inputCode = ""; // Reset the entered code
            }else {
              display.clearDisplay();
              display.setTextSize(2);
              display.setCursor(20, 10);
              display.print("Invalid!");
              display.display();
              //
              xSemaphoreGive(wrong_log);
              //
              vTaskDelay(pdMS_TO_TICKS(1000));
              display.clearDisplay();
              inputCode = ""; // Reset the entered code
            }
          }
        } else if (get_key == 'D') {
          if (inputCode.length() > 0) {
            inputCode.remove(inputCode.length() - 1);
          }
        } else if(inputCode.length()  < 9){
            inputCode += get_key;
          
          }
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(20, 14);
        display.print("Password: ");
        display.print(inputCode);
        display.display();
      }
      if(get_key == '*'){
          inputCode = ""; // Reset the entered code
          display.clearDisplay();
          display.display();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}


void Task_Run_AP  ( void *pvParameters ){
  const char* ssid     = "smart_door_lock";
  const char* password = "12345678";
  WiFiServer server(80);
  String header;
  String output26State = "off";
  const int output26 = LED_BUILTIN;



  WiFi.softAP(ssid, password);
  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 10);
  display.print(WiFi.softAPIP());
  display.display();
  vTaskDelay(pdMS_TO_TICKS(1000));
  pinMode(output26, OUTPUT);
  digitalWrite(output26, LOW);
  server.begin();
  for(;;){
    WiFiClient client = server.available();   // Listen for incoming clients

    if (client) {                             // If a new client connects,
      Serial.println("New Client.");          // print a message out in the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          header += c;
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              
              // turns the GPIOs on and off
              if(header.indexOf("GET /nfc_start") >= 0){
                // nfc tast
                String store_UID;
                Serial.println("nrf_regitster_start!!");
                if (xQueueReceive(UID_String,&store_UID, portMAX_DELAY) == pdTRUE){
                  int Offset = writeStringToEEPROM(200, store_UID); // 50 bytes
                  Serial.println("UID_scan = " + store_UID + "\n length = " + int(Offset - 200));
                  display.clearDisplay();
                  display.setTextSize(2);
                  display.setTextColor(SSD1306_WHITE);
                  display.setCursor(25, 10);
                  display.print("Save card!!");
                  display.display();
                  EEPROM.commit();
                  vTaskDelay(pdMS_TO_TICKS(1000));
                }
              }
              
              else if (header.indexOf("GET /zero/on") >= 0) {
                Serial.println("GPIO 26 on");
                output26State = "on";
                digitalWrite(output26, HIGH);
              } else if (header.indexOf("GET /zero/off") >= 0) {
                Serial.println("GPIO 26 off");
                output26State = "off";
                digitalWrite(output26, LOW);
              }
              else {
                Serial.println("-----------------------");
                Serial.println(header.c_str());
                String SSID = header.substring(header.indexOf("SSID=") + 5 ,header.indexOf("&Password"));
                String Password = header.substring(header.indexOf("Password=") + 9,header.indexOf("&Password_door"));
                String Password_door = header.substring(header.indexOf("Password_door=") + 14,header.indexOf("&Telegram_ID"));
                String Telegram_ID = header.substring(header.indexOf("Telegram_ID=") + 12,header.indexOf(" HTTP"));
                Serial.print("SSID = ");
                Serial.println(SSID);
                Serial.print("Password = ");
                Serial.println(Password);
                Serial.print("Password_door = ");
                Serial.println(Password_door);
                Serial.print("Telegram_ID = ");
                Serial.println(Telegram_ID);
                
                if(SSID.length() > 1){
                  int Offset = writeStringToEEPROM(0, SSID);  // 50 bytes
                  Serial.println("SSID = " + SSID + "\n length = " + Offset);
                }
                if(Password.length() > 1){
                  int Offset = writeStringToEEPROM(50, Password); // 50 bytes
                  Serial.println("Password = " + Password + "\n length = " + int(Offset - 50));
                }
                if(Password_door.length() > 1){
                  int Offset = writeStringToEEPROM(100, Password_door); // 50 bytes
                  Serial.println("Password_door = " + Password_door + "\n length = " + int(Offset - 100));
                }
                if (Telegram_ID.length()>1){
                  int Offset = writeStringToEEPROM(150, Telegram_ID); // 50 bytes
                  Serial.println("Telegram_ID = " + Telegram_ID + "\n length = " + int(Offset - 200));
                }
                if ((SSID.length() > 1) || (Password.length() > 1) || (Password_door.length() > 1) || Telegram_ID.length()>1){
                  EEPROM.commit();
                  display.clearDisplay();
                  display.setTextSize(2);
                  display.setTextColor(SSD1306_WHITE);
                  display.setCursor(25, 10);
                  display.print("Save!!");
                  display.display();
                }
                
              }
              
              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");  
              // CSS to style the on/off buttons 
              // Feel free to change the background-color and font-size attributes to fit your preferences
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555;}</style></head>");

              client.println("<style>.button {border: none;color: white;padding: 16px 32px;text-align: center;text-decoration: none;display: inline-block;font-size:");
              client.println("16px;margin: 4px 2px;transition-duration: 0.4s;cursor: pointer;}");
              client.println(".button1 {background-color: white; color: black; border: 2px solid #04AA6D;}");
              client.println(".button1:hover {background-color: #04AA6D;color: white;}");
              client.println(".button2 {background-color: white; color: black; border: 2px solid #008CBA;}");
              client.println(".button2:hover {background-color: #008CBA;color: white;}</style>");
              // Web Page Heading
              client.println("<body><h1>Testing ESP32 Web Server for smart door lock</h1>");
              
              // Display current state, and ON/OFF buttons for GPIO 26  
              client.println("<p>GPIO zero - State " + output26State + "</p>");
              // If the output26State is off, it displays the ON button       
              if (output26State=="off") {
                client.println("<p><a href=\"/zero/on\"><button class=\"button\">ON</button></a></p>");
              } else {
                client.println("<p><a href=\"/zero/off\"><button class=\"button button2\">OFF</button></a></p>");
              } 
                
              client.println("<form action=\"/smart_door_lock\">");

              client.println("<label for=\"SSID\">SSID:</label>");
              client.println("<input type=\text\" id=\"SSID\" name=\"SSID\" placeholder=\"wifi name\"><br><br>");

              client.println("<label for=\"Password\">Password:</label>");
              client.println("<input type=\text\" id=\"Password\" name=\"Password\" placeholder=\"wifi password\"><br><br>");

              client.println("<label for=\"Password_door\">Password_door:</label>");
              client.println("<input type=\text\" id=\"Password_door\" name=\"Password_door\" placeholder=\"Password_door\"><br><br>");

              client.println("<label for=\"Your Telgram ID\">Telegram_ID:</label>");
              client.println("<input type=\text\" id=\"Telegram_ID\" name=\"Telegram_ID\" placeholder=\"Telegram_ID\"><br><br>");

              client.println("<input type=\"submit\" value=\"Submit\" class=\"button button1\">");
              client.println("</form>");
              client.println("<p><a href=\"/nfc_start\"><button class=\"button button2\">NFC register</button></a></p>"); 
              
              // The HTTP response ends with another blank line
              client.println();
              // Break out of the while loop
              break;
            } else { // if you got a newline, then clear currentLine
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    
  }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
void Task_telegram_bot(void * pvParameters){
  WiFiClientSecure client;
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  UniversalTelegramBot bot(BOTtoken, client);
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  vTaskDelay(pdMS_TO_TICKS(2000));
  while (now < 24 * 3600)
  {
    Serial.print(".");
    vTaskDelay(pdMS_TO_TICKS(100));
    now = time(nullptr);
  }
  for(;;){
    if(xSemaphoreTake(right_log, 10) == pdTRUE){
      Serial.println("telegram!! send");
      bot.sendMessage(Telegram_ID_read, "Your door unlocked", "");
    }
    else if (xSemaphoreTake(wrong_log, 10) == pdTRUE) {
      Serial.println("telegram!! send");
      bot.sendMessage(Telegram_ID_read, "Someone is trying to unlock the door!", "");
    }
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    Serial.print("Free heap: ");
    Serial.println(freeHeap);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void connect(){
  readStringFromEEPROM(0, &SSID_Read);
  readStringFromEEPROM(50, &Password_Read);
  readStringFromEEPROM(100, &Password_door_Read);
  readStringFromEEPROM(200,&Tag_UID_read);
  readStringFromEEPROM(150,&Telegram_ID_read);
  // // convert to char from string
  String(SSID_Read).toCharArray(WIFI_SSID,SSID_Read.length() + 1);
  String(Password_Read).toCharArray(WIFI_PASSWORD,Password_Read.length() + 1);
  //
  Serial.println();
  Serial.println("SSID == " + SSID_Read);
  Serial.println("Password == " + Password_Read);
  Serial.println("Password_door == " + Password_door_Read);
  Serial.println("UID == " + Tag_UID_read);
  Serial.println("Telegram_ID_read == " + Telegram_ID_read);
  // connect wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.print("Connecting to wifi");
  display.display();
  vTaskDelay(pdMS_TO_TICKS(1000));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 10);
  while (WiFi.status() != WL_CONNECTED)
  {
      display.print(".");
      display.display();
      Serial.print(".");
      delay(300);
      if (millis() - ms >= 10000){
        Wifi_status = 0;
        break;
        Serial.println("cant connect wifi");
        // to_connect_wifi = 0;
      }
      else {
        Wifi_status = 1;
        // to_connect_wifi = 1;
      }
  }
  if (Wifi_status == true && to_connect_wifi == 1){
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();


  }
}

int writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  
  return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];

  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0'; // !!! NOTE !!! Remove the space between the slash "/" and "0" (I've added a space because otherwise there is a display bug)

  *strToRead = String(data);
  return addrOffset + 1 + newStrLen;
}

