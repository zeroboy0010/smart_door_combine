#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// eeprom
#include <EEPROM.h>

// keypad
// Define the keypad layout
const byte ROW_NUM = 4; // four rows
const byte COLUMN_NUM = 4; // four columns

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]    =  {27,14,12,13};    // connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = {32,33,25,26};   // connect to the column pinouts of the keypad
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

String inputCode = "";
// end




String Input;
String Output;
String Password;
String SSID;
String Password_door;
String Password_Read;
String SSID_Read;
String Password_door_Read;
int writeStringToEEPROM(int addrOffset, const String &strToWrite);
int readStringFromEEPROM(int addrOffset, String *strToRead);

int config_button = 15;

int connected_wifi;

/* 1. Define the WiFi credentials */
char WIFI_SSID[20];
char WIFI_PASSWORD[20];

bool Wifi_status = true;
bool taskCompleted = false;

// Replace with your network credentials
const char* ssid     = "smart_door_lock";
const char* password = "12345678";
// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;
String output26State = "off";

const int output26 = LED_BUILTIN;

// display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
// Define OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// connect wifi
void connect();
void running_ap();


void setup() {
  EEPROM.begin(150);
  Serial.begin(9600);
  pinMode(config_button,INPUT_PULLUP);
  pinMode(2,OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);  // Don't proceed, loop forever
  }

  for (int i=0; i<30;i++){
    Serial.print(".");
    if (digitalRead(config_button) == 0){
      connected_wifi = 0;
      break;
    }
    else {
      connected_wifi = 1;
    }
    delay(200);
  }

  // WiFi.mode(WIFI_AP_STA);
  if (connected_wifi == 0){
    //
    WiFi.softAP(ssid, password);
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
    //
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(25, 10);
    display.print(WiFi.softAPIP());
    display.display();  // Turn on the screen
    //
    pinMode(output26, OUTPUT);
    digitalWrite(output26, LOW);
    server.begin();
  }
  if (Wifi_status == true && connected_wifi == 1){
    connect();
  }
}

void loop() {

  if (connected_wifi == 0){
    running_ap();
  }
  if(Wifi_status == true && connected_wifi == 1){
    char key = keypad.getKey();
      if (key) {
    if (key == '#') {
      if (inputCode.length()  < 10) {
        if (inputCode == Password_door_Read) {
          display.clearDisplay();
          display.setTextSize(2);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(25, 10);
          display.print("Correct");
          // bot.sendMessage(CHAT_ID, "Someone unlocked the door", "");
          digitalWrite(2, HIGH);
          display.display();
          delay(5000);
          digitalWrite(2, LOW);
          inputCode = ""; // Reset the entered code
        }else {
          display.clearDisplay();
          display.setTextSize(2);
          display.setCursor(20, 10);
          display.print("Invalid!");
          // bot.sendMessage(CHAT_ID, "Someone is tring to unlock the door!", "");
          display.display();
          delay(2000);
          display.clearDisplay();
          inputCode = ""; // Reset the entered code
        }
      }
    } else if (key == 'D') {
      if (inputCode.length() > 0) {
        inputCode.remove(inputCode.length() - 1);
      }
    } else if(inputCode.length()  < 9){
        inputCode += key;
      
      }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 14);
    display.print("Code: ");
    display.print(inputCode);
    display.display();
  }
  if(key == '*'){
      inputCode = ""; // Reset the entered code
      display.clearDisplay();
      display.display();
    }
  }
}

void connect(){
  readStringFromEEPROM(0, &SSID_Read);
  readStringFromEEPROM(50, &Password_Read);
  readStringFromEEPROM(100, &Password_door_Read);
  // // convert to char from string
  String(SSID_Read).toCharArray(WIFI_SSID,SSID_Read.length() + 1);
  String(Password_Read).toCharArray(WIFI_PASSWORD,Password_Read.length() + 1);
  //
  Serial.println();
  Serial.println("SSID == " + SSID_Read);
  Serial.println("Password == " + Password_Read);
  Serial.println("Password_door == " + Password_door_Read);
  // connect wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
      Serial.print(".");
      delay(300);
      if (millis() - ms >= 10000){
        break;
        Serial.println("cant connect wifi");
        connected_wifi = 0;
      }
      else {
        connected_wifi = 1;
      }
  }
  if (Wifi_status == true && connected_wifi == 1){
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();


  }
}

void running_ap(){
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
            
            if (header.indexOf("GET /zero/on") >= 0) {
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
              String Password_door = header.substring(header.indexOf("Password_door=") + 14,header.indexOf(" HTTP"));
              Serial.print("SSID = ");
              Serial.println(SSID);
              Serial.print("Password = ");
              Serial.println(Password);
              Serial.print("Password_door = ");
              Serial.println(Password_door);
              
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
                Serial.println("Password_door = " + Password_door + "\n length = " + int(Offset - 50));
              }
              if ((SSID.length() > 1) || (Password.length() > 1) || (Password_door.length() > 1)){
                EEPROM.commit();
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
               
            client.println("<form action=\"/page_like_counter\">");

            client.println("<label for=\"SSID\">SSID:</label>");
            client.println("<input type=\text\" id=\"SSID\" name=\"SSID\" placeholder=\"wifi name\"><br><br>");

            client.println("<label for=\"Password\">Password:</label>");
            client.println("<input type=\text\" id=\"Password\" name=\"Password\" placeholder=\"wifi password\"><br><br>");

            client.println("<label for=\"Password_door\">Password_door:</label>");
            client.println("<input type=\text\" id=\"Password_door\" name=\"Password_door\" placeholder=\"Password_door\"><br><br>");

            client.println("<input type=\"submit\" value=\"Submit\">");
            client.println("</form>");
            
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