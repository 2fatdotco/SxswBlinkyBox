/*
 * WebSocketClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <Hash.h>
#include <ArduinoJson.h>

#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>


ESP8266WebServer server(80);
WebSocketsServer wsServer = WebSocketsServer(81);

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;


// whitePin = 13;
// redPin = 14;

// Which pin is active right now
int pin = 13;
// Current light level
int level = 0;

int max = 144;
int min = 0;

uint8_t isBusy = 0;
uint8_t doPulse = 0;
uint8_t doFadedown = 0;
uint8_t doFadeup = 0;
uint8_t doFreakout = 0;
uint16_t freakLevel = 10;
uint16_t effectVal = 0;
unsigned long startTime;
unsigned long utNow;
unsigned long moodTimer = 0;
unsigned long lastMilis;

boolean searchForWifi = true;
boolean clientMode = true;
boolean moodMode = false;

String nets;

extern "C" {
  #include "user_interface.h"
}

void setLevel(int num) {
  level = num;

  if (level <= min){
    level = min;
  }
  else {
    if (level >=max){
      level = max;
    }
    
  }
  analogWrite(pin,level);
}

void toggleColor(){
  int lev = level;

  if (pin == 13){
    setLevel(0);
    pin = 14;
  }
  else {
    setLevel(0);
    pin = 13;
  }

  setLevel(lev);
}

void setRed(){
 if (pin == 13){
    toggleColor();
  }
}

void setWhite(){
 if (pin == 14){
    toggleColor();
  }
}

void freakout(int ms) {

  int cL = level;
  setLevel(max);

  uint8_t rate;

  int blinks = freakLevel*int(ms/1000);
  int wait = int(ms/(blinks));
  for(uint8_t c = 0; c <= blinks; c++) {
    toggleColor();
    delay(wait);
  }

  analogWrite(pin,cL);
}

void pulse(int ms) {

  int cL = level;

  int wait = int(ms/(144*2));

  float rate = 1.05;

  for(float c = 1; c < 144; c=rate*c) {
    analogWrite(pin,int(c));
    delay(wait);
  }

  for(float c = 144; c > 1; c=c/rate) {
    analogWrite(pin,int(c));
    delay(wait);
  }

  analogWrite(pin,cL);
}

void fadeup(int ms) {
  int cL = level;

  int wait = int(ms/144);
  

  for(uint8_t c = 1; c < 144; c++) {
    analogWrite(pin,c);
    delay(wait);
  }

  analogWrite(pin,cL);
}

void fadedown(int ms) {
  int cL = level;

  int wait = int(ms/144);  for(uint8_t c = 144; c > 0; c--) {
    analogWrite(pin,c);
    delay(wait);
  }

  analogWrite(pin,cL);
}

void toggleMood(){
    if (moodMode){
      moodMode = false;
  }
  else {
    moodMode = true;
    moodTimer = millis(); 
    pulse(1000);
  }
}

void sendInfo(){
   
   
  String myData = "{\"info\":{";
  myData += "\"mac\":\"" + String(WiFi.macAddress()) + "\",";
  myData += "\"ip\":\"" + String(WiFi.localIP()) + "\",";
  myData += "\"upTime\":" + String(utNow);
  myData += "," + nets;  
  myData += "}}";
  webSocket.sendTXT(myData);
}

void resetUnit(){
   system_restart();
}

void netScan(){
   int n = WiFi.scanNetworks();

  nets = "\"nets\": {";

  for (int i = 0; i < n; ++i){
    if (WiFi.SSID(i)[0] == 'E' && WiFi.SSID(i)[1] == 'S' && WiFi.SSID(i)[2] == 'P' && WiFi.SSID(i)[3] == '_'){
      nets += "\"" + WiFi.SSID(i) + "\":\"" + WiFi.RSSI(i) + "\"";
    }
    if (i != (n-1)){
      nets += ",";
    }
  }
  nets += "}";
}

void parseMessage(char json[]) {
   StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(json);

  // Test if parsing succeeds.
  if (!root.success()) {
    return;
  }

  if (root["level"]){
    setLevel(root["level"]);
  }

  if (root["scan"]){
    netScan();
  }

  if (root["info"]){
    sendInfo();
  }

  if (root["toggleDP"]){
    digitalWrite(root["toggleDP"], !digitalRead(root["toggleDP"]));
  }

  if (root["pulse"]){
    doPulse = 1;
    effectVal = root["pulse"];
  }

  if (root["fadeup"]){
    doFadeup = 1;
    effectVal = root["fadeup"];
  }

  if (root["fadedown"]){
    doFadedown = 1;
    effectVal = root["fadedown"];
  }

  if (root["freakout"]){
    doFreakout = 1;
    effectVal = root["freakout"];
  }

  if (root["setFreakLevel"]){
    freakLevel = root["setFreakLevel"];
  }

  if (root["toggleColor"]){
    toggleColor();
  }

  if (root["reset"]){
    resetUnit();
  }

  if (root["toggleMood"]){
    toggleMood();
  }

  if (root["setRed"]){
    setRed();
  }

  if (root["setWhite"]){
    setWhite();
  }

}

void wsServerEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    switch(type) {
        case WStype_DISCONNECTED:
                        break;
        case WStype_CONNECTED: {
            setLevel(10);
            IPAddress ip = wsServer.remoteIP(num);
                    }
            break;
        case WStype_TEXT: 
            {
             parseMessage((char *)payload);
            }
            break;
    }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:

            break;
        case WStype_CONNECTED:
            {
            String myData = "{\"firstConnect\":true}";
            webSocket.sendTXT(myData);
            setLevel(10);

            }
            break;
        case WStype_TEXT: 
            {
             parseMessage((char *)payload);
            }
            break;
    }
}

void serveHome() {
  String top = "<!DOCTYPE html><html><head><title>Blinky Box</title><style>body {text-align:center;width:100%;}.b{cursor:pointer;border:2px solid black;border-radius:15px;height: 50px;width: 160px;padding-top: 20px;margin: 20px 50px;}#wb{background-color:lightblue;}table {max-width:800px;margin: auto;}p {font-family: \"Lucida Console\", Monaco, monospace;}</style></head>";
  String middle = "<script language=\"javascript\">var props = {level: 10,pulse: 2000,fadeup: 2000,fadedown: 2000,setFreakLevel: 15,toggleMood: false};var connection;var setProp = function(elem){props[elem.id] = Number(elem.value);var msg = {};msg[elem.id] = Number(elem.value);connection.send(JSON.stringify(msg));if (elem.id === 'setFreakLevel'){setTimeout(function(){connection.send('{\"freakout\":3000}');},1000);}};var printIt = function(event){var infoBox = document.getElementById('infoBox');if (!!infoBox){infoBox.innerHTML = event&&event.data || \"\";}};function makeConnection(){console.log('Connecting to websocket');connection = new WebSocket('ws://192.168.4.1:81/', ['arduino']);connection.onopen = function(){console.log('Connected.');};connection.onclose = printIt;connection.onerror = printIt;connection.onmessage = printIt;}var bbg = function(cid,col){var e = document.getElementById(cid);var c = e.style;c.backgroundColor = col;};var red = function(lev){bbg('rb','lightblue');bbg('wb','white');connection.send(\"{\\\"setRed\\\":true}\");};var white = function(lev){bbg('wb','lightblue');bbg('rb','white');connection.send(\"{\\\"setWhite\\\":true}\");};var toggleMood = function(){props['toggleMood'] = !!!props['toggleMood'];if (props['toggleMood']){bbg('mood','red');}else {bbg('mood','white');}connection.send(\"{\\\"toggleMood\\\":\"+props['toggleMood']+\"}\");};document.onload = makeConnection();</script>";
  String bottom = "<body><div id=\"infoBox\" style=\"text-align:center;font-size:2em;\"></div><h1> Blinky Box </h1><table><td><div id=\"rb\" class=\"b\" onClick=\"red()\">Red</div><div id=\"wb\" class=\"b\" onClick=\"white()\">White</div><div id=\"mood\" class=\"b\" onClick=\"toggleMood()\">Set the mood</div></td><td><p>Light Level</p><input id=\"level\" type=\"range\" min=\"-1\" max=\"144\" step=\"1\" value=\"10\" onchange=\"setProp(this)\"><br><p>Pulse</p><input id=\"pulse\" type=\"range\" min=\"250\" max=\"10000\" step=\"250\" value=\"2000\" onchange=\"setProp(this)\"><br><p>Fadeup</p><input id=\"fadeup\" type=\"range\" min=\"250\" max=\"10000\" step=\"250\" value=\"2000\" onchange=\"setProp(this)\"><br><p>Fadedown</p><input id=\"fadedown\" type=\"range\" min=\"250\" max=\"10000\" step=\"250\" value=\"2000\" onchange=\"setProp(this)\"><br><p>Freakout</p><input id=\"setFreakLevel\" type=\"range\" min=\"2\" max=\"30\" step=\"1\" value=\"3\" onchange=\"setProp(this)\"><br></td></table><br><br><br><h3>See how we made it</h3><p>http://sxsw.2fat.co </p><h3>See the code on Github</h3><p>https://github.com/2fatdotco/blinkyBox </p><h3>Buy one for your mum</h3><p>http://shop.2fat.co</p><h3>Be our friend on Twitter</h3><p>@2fatdotco</p><h3>Get in touch</h3><p>info@2fat.co</p></body></html>";
  server.send(200, "text/html", (top+middle+bottom));
}

char* getNetName(){
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);

    String mid = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    String netName = "BlinkyBox"+mid;
    char netNameChar[netName.length() + 1];
    memset(netNameChar, 0, netName.length() + 1);

    for (int i=0; i<netName.length(); i++){
      netNameChar[i] = netName.charAt(i);      
    }

    return netNameChar;
}


void setupServer(){
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAP((getNetName()));
    IPAddress myIP = WiFi.softAPIP();

    wsServer.begin();
    wsServer.onEvent(wsServerEvent);

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);

    server.on("/", serveHome);
    server.onNotFound ( serveHome );

    server.begin();
}


void setupClient(){
    webSocket.begin("192.168.0.166", 1440,"/",String(WiFi.macAddress()));
    webSocket.onEvent(webSocketEvent);
}


void setup() {
    // Serial.begin(115200);

    analogWriteRange(max);

    // Serial.setDebugOutput(false);

    for(uint8_t t = 3; t > 0; t--) {
        delay(100);
    }

    netScan();

    WiFi.mode(WIFI_STA);

    WiFiMulti.addAP("bruceSterlingsToaster", "dickButtJelly");

    while(searchForWifi == true && (WiFiMulti.run() != WL_CONNECTED)) {
      
      if (startTime == 0){
        startTime = millis();
      }
      utNow = startTime - millis();

      if (utNow >= 60000){
        clientMode = false;
        searchForWifi = false;
        setupServer();
      }
      delay(100);
    }

    if (clientMode){
      setupClient();
    }

}

void loop() {

  if (!isBusy){
    isBusy = 1;

    if (doPulse){
      pulse(effectVal);
    }

    if (doFadeup){
      fadeup(effectVal);
    }

    if (doFadedown){
      fadedown(effectVal);
    }

    if (doFreakout){
      freakout(effectVal);
    }

    doFreakout = 0;
    isBusy = 0;
    doFreakout = 0;
    doPulse = 0;
    doFadeup = 0;
    doFadedown = 0;
  }

  if (moodMode){
    moodTimer = moodTimer + (millis() - lastMilis);
    if (moodTimer >= 4000){
    
       long randNumber;
       long effectVal;
       randNumber = random(2000,4000);
       randNumber = random(1,10);

        if (randNumber == 1 || randNumber == 2){
          effectVal = randNumber;
          doFadeup = 1;
        }

        if (randNumber == 3 || randNumber == 4){
          effectVal = 3000;
          doFadedown = 1;
        }

        if (randNumber == 5 || randNumber == 6){
          effectVal = randNumber;
          doPulse = 1;
        }

        if (randNumber == 7){
          toggleColor();
        }

      moodTimer = 0;
    }
  }

  if (!clientMode){
    wsServer.loop();
    server.handleClient();
  }

  lastMilis = millis();

}

