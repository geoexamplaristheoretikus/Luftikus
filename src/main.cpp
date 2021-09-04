// Example testing sketch for various DHT humidity/temperature sensors
// Written by ladyada, modified for integer use, public domain
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DHTesp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "./functions.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <Effortless_SPIFFS.h>
#include <time.h>
#include <ESPAsyncWiFiManager.h> 
#define DHTGPIOOUT 5
#define DHTGPIOIN 14
#define oneWireBus 2
#define RelaisGPIO 16
#define DHTTYPE DHT22

eSPIFFS fileSystem;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature ds18(&oneWire);
// Setup DHT22 sensors.
DHTesp dhtout;
DHTesp dhtin;

// Sensor readings variables
float Wall_Temperature;
float Indoor_Humidity;
float Indoor_Temperature;
float Outdoor_Humidity;
float Outdoor_Temperature;
// Computed variables
float Outdoor_Absolute_Humidity;
float Indoor_Absolute_Humidity;
float Dewpoint_Outdoor;
float Outdoor_Humidity_err;
float Indoor_Humidity_err;
float Dewpoint_Outdoor_err;
float out_hum_err;
float in_hum_err;
// User setable decision variables
float minium_basem_t = 5.0;
float minium_basem_h = 0.0;
// Strings for descision visualisation on website
String conditionabs;
String conditiondew;
String conditionmin;
// Bool for decision of ventilation
bool abscond;
bool dewcond;
bool mincond;
bool fanon = false;
// timekeeping variables
int totalruntime = 0;
int totalhours = 0;
int totalweeks = 0;
time_t timercont = 0;
String starttimelastrun;
String timeinfo;
String endtimelastrun;
String totalt;
String timelist[200];
// Strings for website header
String systemOff ="<span style=\"color: #FF0000\"> Luftikus OFF</span>";
String systemOn = "<span style=\"color: #008000\"> &#x1F32C;Luftikus ON </span>";
String systemonoff = systemOff;

const char* PARAM_INPUT_1 = "input1";
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";

struct tm tm;         // http://www.cplusplus.com/reference/ctime/tm/

const uint32_t SYNC_INTERVAL = 24;              // NTP Sync Interval in Stunden
const uint32_t Monat_INTERVAL = 30;              // NTP Sync Interval in Stunden

const char* const PROGMEM NTP_SERVER[] = {"de.pool.ntp.org", "at.pool.ntp.org", "ch.pool.ntp.org", "ptbtime1.ptb.de", "europe.pool.ntp.org"};

extern "C" uint8_t sntp_getreachability(uint8_t);

bool getNtpServer(bool reply = false) {
  uint32_t timeout {millis()};
  configTime("CET-1CEST,M3.5.0/02,M10.5.0/03", NTP_SERVER[0], NTP_SERVER[1], NTP_SERVER[2]);  
  do {
    delay(25);
    if (millis() - timeout >= 1e3) {
      Serial.printf("Wait for NTP repsonse %02ld sec\n", (millis() - timeout) / 1000);
      delay(975);
    }
    sntp_getreachability(0) ? reply = true : sntp_getreachability(1) ? reply = true : sntp_getreachability(2) ? reply = true : false;
  } while (millis() - timeout <= 16e3 && !reply);
  return reply;
}


// wifimanager and webserver setup
void configModeCallback (AsyncWiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}
AsyncWebServer server(80);
DNSServer dns;

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}
String SendHTML(float T0,
                float T1,float H1, float A1, float E1,
                float T2,float H2, float A2, float E2, float D2, float ED2,
                String textabs, String textdew, String textmin, String onoff, String sstart, String sstop, String alt) {
    String ptr = "<!DOCTYPE html> <html>\n";
    ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, charset=utf-8\">\n";
    ptr +="<title>Luftikus</title>\n";
    ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
    ptr +="p {font-size: 16px;color: #444444;margin-bottom: 10px;}\n";
    ptr +="</style>\n";
    ptr +="</head>\n";
    ptr +="<body>\n";
    ptr +="<div id=\"webpage\">\n";
    ptr +="<h1>";
    ptr +=(String)onoff;
    ptr +="</h1>\n";
    ptr +=(String)sstart;
    ptr +="\n";
    ptr +="<p>Total runtime: <b>";
    ptr +=(String)alt;
    ptr +="</b><p>\n";
    ptr +="<h2>Conditions satisfied?</h2>\n";
    ptr +="<p>Outdoor humidity < indoor humidity?</p>\n";
    ptr +="<p><TT>";
    ptr +=(String)textabs;
    ptr +="</TT></p>";
    ptr +="<p>Outdoor dew point < wall temperature?</p>\n";
    ptr +="<p><TT>";
    ptr +=(String)textdew;
    ptr +="</TT></p>";
    ptr +="<p>Indoor temperature higher than minimum set?";
    ptr +="</p>\n";
    ptr +="<p><TT>";
    ptr +=(String)textmin;
    ptr +="</TT></p>";
    ptr +="<h2>Sensor Readings</h2>\n";
    ptr +="<p>Outdoor: <TT>";
    ptr +=(float)T2;
    ptr +="&deg;C, ";
    ptr +=(float)H2;
    ptr +="%</TT></p>\n";
    ptr +="<p>Indoor: <TT>";
    ptr +=(float)T1;
    ptr +="&deg;C, ";
    ptr +=(float)H1;
    ptr +="%</TT></p>\n";
    ptr +="<p>Wall: <TT>";
    ptr +=(float)T0;
    ptr +="&deg;C";
    ptr +="</TT></p>";
    ptr +="<h2>User Config</h2>Minimum indoor temperature:\n";
    ptr +="<form action=\"/get\">";
    ptr +="<input type=\"number\" min=\"-42\" max=\"42\" step=\"0.01\" name=\"input1\">";
    ptr +="<input type=\"submit\" value=\"Submit\">";
    ptr +="<br> <TT>(";
    ptr +=(float)minium_basem_t;
    ptr +="&deg;C)</TT></form></div>\n Minimum indoor humidity:\n";
    ptr +="<form action=\"/get\">";
    ptr +="<input type=\"number\" min=\"0\" max=\"100\" step=\"0.01\" name=\"input2\">";
    ptr +="<input type=\"submit\" value=\"Submit\">";
    ptr +="<br> <TT>(";
    ptr +=(float)minium_basem_h;
    ptr +="%)</TT></form></div>\n";
    ptr +="<form action=\"/get\">";
    ptr +="\n \n ";
    ptr +="<input type=\"submit\" value=\"Fan for 10 minutes on.\" name=\"input3\" >";
    ptr +="</form></div>\n";
    ptr +="</body>\n";
    ptr +="</html>\n";
    return ptr;
}

void setup() {
    //setup serial
    Serial.begin(9600); // Output status on Uno serial monitor
    Serial.printf("\n\nSketchname: %s\nBuild: %s\t\tIDE: %d.%d.%d\n%s\n\n",
                (__FILE__), (__TIMESTAMP__), ARDUINO / 10000, ARDUINO %
                10000 / 100, ARDUINO % 100 / 10 ? ARDUINO % 100 : ARDUINO % 10,
                ESP.getFullVersion().c_str());
    delay(100);

    
    //setup relaispin 
    pinMode(RelaisGPIO, OUTPUT);
    digitalWrite(RelaisGPIO, LOW);
    
    
    // setup sensors
    Serial.println("Initialize Sensors");
    dhtout.setup(DHTGPIOOUT, DHTesp::DHT22);
    dhtin.setup(DHTGPIOIN, DHTesp::DHT22);
    ds18.begin();

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFi.persistent(true); 
    AsyncWiFiManager wifiManager(&server,&dns);
    //reset settings - for testing
    //wifiManager.resetSettings();

    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if(!wifiManager.autoConnect("Luftikus-Setup-AP")) {
      Serial.println("failed to connect and hit timeout");
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
  }


    //connect to your local wi-fi network

    //WiFi.begin(WLANSSID, PASSWORD);

    //check wi-fi is connected to wi-fi network
    // while (WiFi.status() != WL_CONNECTED) {
    //   delay(1000);
    //    Serial.print(".");
    // }
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
    

    // get time after connect
    bool timeSync = getNtpServer();
    Serial.printf("NTP Synchronisation %s!\n", timeSync ? "erfolgreich" : "fehlgeschlagen");
    

    // Send web page with input fields to client
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(200, "text/html", SendHTML(
                                           Wall_Temperature,
                                           Indoor_Temperature,
                                           Indoor_Humidity,
                                           Indoor_Absolute_Humidity,
                                           Indoor_Humidity_err,
                                           Outdoor_Temperature,
                                           Outdoor_Humidity,
                                           Outdoor_Absolute_Humidity,
                                           Outdoor_Humidity_err,
                                           Dewpoint_Outdoor,
                                           Dewpoint_Outdoor_err,
                                           conditionabs,
                                           conditiondew,
                                           conditionmin,
                                           systemonoff,
                                           timeinfo,
                                           endtimelastrun,
                                           totalt) );});
     

    // Send a GET request to <ESP_IP>/get?input1=<inputMessage>

    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      minium_basem_t = inputMessage.toFloat();
      if (fileSystem.saveToFile("/baset.txt", minium_basem_t)) {
        Serial.println("Successfully wrote data to file");
        } 
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      minium_basem_h = inputMessage.toFloat();
      if (fileSystem.saveToFile("/baseh.txt", minium_basem_h)) {
        Serial.println("Successfully wrote data to file");
        } 
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
      fanon = true;
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
    });
    

    server.onNotFound(notFound);
    server.begin();

    Serial.println("HTTP server started");
    //fileSystem.openFromFile("/totaltime.txt", totalruntime);
    // Open the storage file and save data to myVariable
    //  load persistent variables from file
    if (fileSystem.openFromFile("/totalruntime.txt", totalruntime)) {
        Serial.print("Successfully read file and parsed data: ");
        Serial.println(totalruntime);}
    if (fileSystem.openFromFile("/totalhours.txt", totalhours)) {
        Serial.print("Successfully read file and parsed data: ");
        Serial.println(totalhours);} 
    if (fileSystem.openFromFile("/totalweeks.txt", totalweeks)) {
        Serial.print("Successfully read file and parsed data: ");
        Serial.println(totalweeks);}       
    if (fileSystem.openFromFile("/baset.txt", minium_basem_t)) {
        Serial.print("Successfully read file and parsed data: ");
        Serial.println(totalruntime);} 
    if (fileSystem.openFromFile("/baseh.txt", minium_basem_h)) {
        Serial.print("Successfully read file and parsed data: ");
        Serial.println(totalruntime);}  

}
void loop() {
    //setup system-time
    char buffer[25];
    static time_t lastsec {0};
    time_t now = time(&now);
    localtime_r(&now, &tm);
    if (tm.tm_sec != lastsec) {
      lastsec = tm.tm_sec;
      strftime (buffer, 25, "%c", &tm);
      if (!(time(&now) % (SYNC_INTERVAL * 3600))) {
        getNtpServer(true);
      }} 




    // read wall sensor
    ds18.requestTemperatures();
    Serial.println("Requesting wall-temperature from the DS18-Sensor.");
    Wall_Temperature = ds18.getTempCByIndex(0);
    Serial.println(Wall_Temperature);

    
    
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    Serial.println("Requesting temperature and humidity from the DS22-Sensors.");
    Indoor_Humidity = dhtout.getHumidity();
    in_hum_err = humidity_err(Indoor_Humidity);
    Indoor_Temperature = dhtout.getTemperature();
    Outdoor_Humidity = dhtin.getHumidity();
    out_hum_err = humidity_err(Outdoor_Humidity);
    Outdoor_Temperature = dhtin.getTemperature();
    Outdoor_Temperature *= 0.2; // for testing


    // Compute important meteorological values for decision
    Indoor_Absolute_Humidity = compabshum(Indoor_Temperature, Indoor_Humidity);
    Indoor_Humidity_err = Indoor_Absolute_Humidity -
        compabshum(Indoor_Temperature-0.5,
                   fmax(0.0, Indoor_Humidity - humidity_err(Indoor_Humidity)));
    Outdoor_Absolute_Humidity =
        compabshum(Outdoor_Temperature, Outdoor_Humidity);
    Outdoor_Humidity_err =
        compabshum(Outdoor_Temperature+0.5,
                   fmin(100.0,
                        Outdoor_Humidity + humidity_err(Outdoor_Humidity)))
        - Outdoor_Absolute_Humidity;
    Dewpoint_Outdoor = compdewpoint(Outdoor_Temperature, Outdoor_Humidity);
    Dewpoint_Outdoor_err =
        compdewpoint(Outdoor_Temperature + 0.5,
                   fmin(100.0,
                        Outdoor_Humidity + humidity_err(Outdoor_Humidity)))
                   - Dewpoint_Outdoor ;

    // Compare values and decicide
    // 1. Compare absolute humidity
    if ((Indoor_Absolute_Humidity - Indoor_Humidity_err) <
        (Outdoor_Absolute_Humidity + Outdoor_Humidity_err))
    {abscond = false;
        conditionabs = "<p><span style=\"color: #FF0000\">"
            + String(Outdoor_Absolute_Humidity)
            + "g/m<sup>3</sup>(+"
            +  String(Outdoor_Humidity_err)
            + ")"
            + " &#8814;  "
            + String(Indoor_Absolute_Humidity)
            + "g/m<sup>3</sup>(-"
            + String(Indoor_Humidity_err)
            + ")   &#10060;</span></p>";}
    else {abscond = true;
        conditionabs = "<p><span style=\"color: #008000\">"
            + String(Outdoor_Absolute_Humidity)
            + "g/m<sup>3</sup>(+"
            +  String(Outdoor_Humidity_err)
            + ")"
            + "  <  "
            + String(Indoor_Absolute_Humidity)
            + "g/m<sup>3</sup>(-"
            + String(Indoor_Humidity_err)
            + ")  &#10003;</span></p>";}
    // 2. Check that dew point outdoors is smaller than wall temp.
    if ((Dewpoint_Outdoor + Dewpoint_Outdoor_err) >
        (Wall_Temperature - 0.5))
    {dewcond = false;
        conditiondew = "<p><span style=\"color: #FF0000\">"
            + String(Dewpoint_Outdoor)
            + "&deg;C(+"
            +  String(Dewpoint_Outdoor_err)
            + ")"
            + "  &#8814;  "
            + String(Wall_Temperature)
            + "&deg;C(-"
            + String(0.5)
            + ")   &#10060;</span></p>";}
    else {dewcond = true;
        conditiondew = "<p><span style=\"color: #008000\">"
            + String(Dewpoint_Outdoor)
            + "&deg;C(+"
            +  String(Dewpoint_Outdoor_err)
            + ")"
            + "  <  "
            + String(Wall_Temperature)
            + "&deg;C(-"
            + String(0.5)
            + ")  &#10003;</span></p>";}
    // 3. Indoor temp high and humidity enough ??
    if ((Indoor_Temperature - 0.5) < minium_basem_t  ||
        (Indoor_Humidity - Indoor_Humidity_err) < minium_basem_h)
    {mincond = false;
        conditionmin = "<p><span style=\"color: #FF0000\"> "
            + String(minium_basem_t)
            + "&deg;C &#8814; "
            + String(Indoor_Temperature)
            + "&deg;C(-"
            + String(0.5)
            + ")  &#10060; </span></p>";}
    else {mincond = true;
        conditionmin = "<p><span style=\"color: #008000\"> "
            + String(minium_basem_t)
            +"&deg;C < "
            + String(Indoor_Temperature)
            + "&deg;C(-"
            + String(0.5)
            + ")  &#10003;</span></p>";}




    // Check condition and switch relais if necessFary
    if (abscond && dewcond && mincond){
      if(digitalRead(RelaisGPIO) == HIGH){
          Serial.println("Conditions are still right, keeping it turning.");
          //store time with sensor on // als funktion umwandeln
          time_t nowish = time(&now);
          totalruntime += nowish - timercont;
              timercont = nowish;
          if (totalruntime >= 3600) {
            totalhours += totalruntime/3600;
            totalruntime = totalruntime/3600;
            if (totalhours > 7*24){
            totalweeks += 1;
            totalhours += -7*24;  
            }
          }
             }
        else{
          digitalWrite(RelaisGPIO, HIGH);
          Serial.println("Conditions turned good, turning fan on.");
          systemonoff = systemOn;
          time_t nowish = time(&now);
          timercont = nowish;
          localtime_r(&nowish, &tm);
          strftime (buffer, 25, "%c", &tm);
          timeinfo =  "Run started from <b>";
          timeinfo += buffer;
          timeinfo += ".</b>";
          starttimelastrun = buffer;
          String slr ="Start:";
          slr += buffer; 
          int i;
          for (i=0; i<200; i++){
            timelist[i+1]=timelist[i]; 
          }
          timelist[0]= slr; 
          endtimelastrun = "-";
          Serial.println(buffer);
          Serial.println(now);
          Serial.println(starttimelastrun);
          }}
    else
    {if(digitalRead(RelaisGPIO) == LOW)
     {Serial.println("Conditions are still not right, keeping it off.");}
      else{Serial.println(RelaisGPIO);
        digitalWrite(RelaisGPIO, LOW);
           Serial.println("Conditions turned not right, switching it off.");
           systemonoff = systemOff;
           timeinfo =  "Last run was from <b>";
           timeinfo += starttimelastrun;
           timeinfo += "</b> to <b>";
           time_t nowish = time(&now);
           totalruntime += nowish - timercont;
           if (totalruntime >= 3600) {
            totalhours += totalruntime/3600;
            totalruntime = totalruntime/3600;
            if (totalhours > 7*24){
            totalweeks += 1;
            totalhours += -7*24;  
            }
            }           
           localtime_r(&nowish, &tm);
           strftime (buffer, 25, "%c", &tm);
           timeinfo += buffer;
           timeinfo += "</b>.";
           String slr ="Ende:";
           slr += buffer; 
           int i;
           for (i=0; i<200; i++){
             timelist[i+1]=timelist[i]; 
           }
           timelist[0]= slr; 
         }} 

    
    // Compute Time run for humans 
    int sek = totalruntime % 60;
    int minut = (totalruntime - sek) / 60;
    int stund = totalhours % 24;
    int tag = (totalhours-stund) / 24;

    totalt =  "Weeks: ";
    totalt +=(int)totalweeks;
    totalt +=  " Days: ";
    totalt +=(int)tag;
    totalt +=  " Hours: ";
    totalt +=(int)stund;
    totalt +=  " Minutes: ";
    totalt +=(int)minut;

    // Write the data back to the SPIFFS
    if (fileSystem.saveToFile("/totalruntime.txt", totalruntime)) {
        Serial.println("Successfully wrote data to file");
        } 
    if (fileSystem.saveToFile("/totalhours.txt", totalhours)) {
        Serial.println("Successfully wrote data to file");
        } 
    if (fileSystem.saveToFile("/totalweeks.txt", totalweeks)) {
        Serial.println("Successfully wrote data to file");
        }      

    if (fanon == true){
      digitalWrite(RelaisGPIO, HIGH);
      fanon = false;
      delay(600000);
      digitalWrite(RelaisGPIO, LOW);
    }     
  
    

               
}
