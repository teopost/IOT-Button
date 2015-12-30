/*
 * IOT Button
 * 
 * Intenet of thing button based on esp8266
 *
 * Thanks to: 
 * 
 * - http://blog.eikeland.se/2015/07/20/coffee-button/
 * - https://github.com/garthvh/esp8266button
 * 
 * and:
 * 
 * - Cesare (Cece) Gridelli for hardware support
 * - Christian (Gallo) Galeffi for positive strokes
 */

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <DNSServer.h>

#define RETRYCOUNT 3
#define SPININTERVAL 0.09
#define LEDCOUNT 3

// Pin definition
// --------------
#define SETUP_PIN 5
const int LEDPINS[] = { 12 };

// IFTTT Definitions
// -----------------
const char* IFTTT_URL= "maker.ifttt.com";
const char* IFTTT_KEY= "dR7obzZI6bqblJX5nRpl8YA";
const char* IFTTT_EVENT = "button";
const char* IFTTT_NOTIFICATION_EVENT = "iot-event";

// Prototypes
// ----------
ESP8266WebServer WEB_SERVER(80);
void setupMode();
boolean triggerButtonEvent(String eventName);

// Global variables
// ---------------
String ssid = "";
String password = "";
String DEVICE_TITLE = "ESP8266 Smart Button";
String SSID_LIST = "";
boolean configMode = false;
Ticker spinticker;
volatile int spincount;

// Procedures
// ==========
boolean loadSavedConfig() { 
  Serial.println("Reading Saved Config....");
  
  if (EEPROM.read(0) != 0) {
    for (int i = 0; i < 32; ++i) {
      ssid += char(EEPROM.read(i));
    }
    Serial.print("SSID: ");
    Serial.println(ssid);
    
    for (int i = 32; i < 96; ++i) {
      password += char(EEPROM.read(i));
    }
    
    Serial.print("Password: ");
    Serial.println(password);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    return true;
  }
  else {
    Serial.println("Saved Configuration not found.");
    return false;
  }
}

void spinLEDs() {
    spincount++;

    for (int i = 0; i < LEDCOUNT; i++) {
        if (spincount % LEDCOUNT == i) {
            digitalWrite(LEDPINS[i], HIGH);
        } else {
            digitalWrite(LEDPINS[i], LOW);
        }
    }
}

void stopSpinner() {
    spinticker.detach();
    for (int i = 0; i < LEDCOUNT; i++) {
        digitalWrite(LEDPINS[i], LOW);
    }
}

void flashAll(int times, int delayms) {
    int blinkCount = times;

    while (blinkCount-- > 0) {
        for (int led = 0; led < LEDCOUNT; led++) {
            if (blinkCount % 2 == 0) {
                digitalWrite(LEDPINS[led], LOW);
            } else {
                digitalWrite(LEDPINS[led], HIGH);
            }
        }
        delay(delayms);
    }

}

boolean checkWiFiConnection() {
  int count = 0;
  Serial.print("Waiting to connect to the specified WiFi network");
  
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  return false;
}

void powerOff() {
    Serial.println("Power OFF");
    delay(250);
    ESP.deepSleep(0, WAKE_RF_DEFAULT);
}

// ======
//  MAIN
// ======
void setup() {

    // Open serial port for debug
    Serial.begin(115200);
    // Init EEPROM
    EEPROM.begin(512);

    // Initialize spin leds
    for (int i = 0; i < LEDCOUNT; i++) {
        pinMode(LEDPINS[i], OUTPUT);
        digitalWrite(LEDPINS[i], LOW);
    }

    // Execute (attach) spinLEDs every SPININTERVAL mills
    spinticker.attach(SPININTERVAL, spinLEDs);

    // If SETUP_PIN pushed, enter to setup mode
    if(digitalRead(SETUP_PIN) == 0) {
       Serial.print("Enter setup mode");
       stopSpinner();
       flashAll(90, 100);
       setupMode();
       return; 
       
    }
    
    // If configuration loading failed, enter to setup mode
    if (!loadSavedConfig()) {
        Serial.print("WARNING: Non trovo una configurazione salvata");
        
        stopSpinner();
        flashAll(90, 100);
        setupMode();
        return;  
    }
    
    // If WIFI check failing, enter to setup mode
    if (!checkWiFiConnection()) {
        Serial.print("ERROR: Non riesco a collegarmi alla wifi");
  
        stopSpinner();
        flashAll(40, 100);
        powerOff();
        return;
    }

    // Se sono qui e' perchè sono riusco a collegami a internet
    
    // Faccio 3 tentativo per mandare la mia chiamata
    for (int i = 0; i < RETRYCOUNT; i++) {
      // Se la chiamata va a buon fine
        if (triggerButtonEvent(IFTTT_EVENT)) {
            // ... esco dal ciclo
            break;
        } else if (i == RETRYCOUNT - 1) {
            // Se dopo 3 tentativi non riesco a collegarmi a internet, spengo i leg
            stopSpinner();
            // Lampeggio in modo diverso
            flashAll(20, 500);
        }
    }

     // Se sono qui è perchè ho già effettuato la chiamata su internet
    stopSpinner();
    // Faccio un piccolo lampeggio
    flashAll(10, 300);
    // .. e mi spengo
    powerOff();
}



// Build the SSID list and setup a software access point for setup mode
// --------------------------------------------------------------------

// HTML Page maker
// ---------------
String makePage(String title, String contents) {
  
  
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<style>";
  // Simple Reset CSS
  s += "*,*:before,*:after{-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}html{font-size:100%;-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}html,button,input,select,textarea{font-family:sans-serif}article,aside,details,figcaption,figure,footer,header,hgroup,main,nav,section,summary{display:block}body,form,fieldset,legend,input,select,textarea,button{margin:0}audio,canvas,progress,video{display:inline-block;vertical-align:baseline}audio:not([controls]){display:none;height:0}[hidden],template{display:none}img{border:0}svg:not(:root){overflow:hidden}body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}";
  // Basic CSS Styles
  s += "body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}h1{font-size:32px;font-size:2rem;line-height:38px;line-height:2.375rem;margin-top:0.7em;margin-bottom:0.5em;color:#343434;font-weight:400}fieldset,legend{border:0;margin:0;padding:0}legend{font-size:18px;font-size:1.125rem;line-height:24px;line-height:1.5rem;font-weight:700}label,button,input,optgroup,select,textarea{color:inherit;font:inherit;margin:0}input{line-height:normal}.input{width:100%}input[type='text'],input[type='email'],input[type='tel'],input[type='date']{height:36px;padding:0 0.4em}input[type='checkbox'],input[type='radio']{box-sizing:border-box;padding:0}";
  // Custom CSS
  s += "header{width:100%;background-color: #2c3e50;top: 0;min-height:60px;margin-bottom:21px;font-size:15px;color: #fff}.content-body{padding:0 1em 0 1em}header p{font-size: 1.25rem;float: left;position: relative;z-index: 1000;line-height: normal; margin: 15px 0 0 10px}";
  s += "</style>";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += "<header><p>" + DEVICE_TITLE + "</p></header>";
  s += "<div class=\"content-body\">";
  s += contents;
  s += "</div>";
  s += "</body></html>";
  return s;
}

// Decode URL
// ----------
String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

void setupMode() {
  
  configMode = true;
   
  DNSServer DNS_SERVER;
  const char* AP_SSID = "ESP8266_BUTTON_SETUP";
  const IPAddress AP_IP(192, 168, 1, 1);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  
  Serial.println("");
  for (int i = 0; i < n; ++i) {
    SSID_LIST += "<option value=\"";
    SSID_LIST += WiFi.SSID(i);
    SSID_LIST += "\">";
    SSID_LIST += WiFi.SSID(i);
    SSID_LIST += "</option>";
  }
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);
  DNS_SERVER.start(53, "*", AP_IP);

  // ----------------------
  
   Serial.print("Starting Access Point at \"");
   Serial.println(WiFi.softAPIP());
    
    // Settings Page
    WEB_SERVER.on("/settings", []() {
      String s = "<h2>Wi-Fi Settings</h2><p>Please select the SSID of the network you wish to connect to and then enter the password and submit.</p>";
      s += "<form method=\"get\" action=\"setap\"><label>SSID: </label><select name=\"ssid\">";
      s += SSID_LIST;
      s += "</select><br><br>Password: <input name=\"pass\" length=64 type=\"password\"><br><br><input type=\"submit\"></form>";
      WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
    });
    
    // setap Form Post
    WEB_SERVER.on("/setap", []() {
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      String ssid = urlDecode(WEB_SERVER.arg("ssid"));
      Serial.print("SSID: ");
      Serial.println(ssid);
      
      String pass = urlDecode(WEB_SERVER.arg("pass"));
      Serial.print("Password: ");
      Serial.println(pass);
      
      Serial.println("Writing SSID to EEPROM...");
      for (int i = 0; i < ssid.length(); ++i) {
        EEPROM.write(i, ssid[i]);
      }
      
      Serial.println("Writing Password to EEPROM...");
      for (int i = 0; i < pass.length(); ++i) {
        EEPROM.write(32 + i, pass[i]);
      }
      EEPROM.commit();
      Serial.println("Write EEPROM done!");
      String s = "<h1>WiFi Setup complete.</h1><p>The button will be connected automatically to \"";
      s += ssid;
      s += "\" after the restart.";
      WEB_SERVER.send(200, "text/html", makePage("Wi-Fi Settings", s));
      //ESP.restart();

      stopSpinner();
      powerOff();
       
    });
 
    // Show the configuration page if no path is specified
    WEB_SERVER.onNotFound([]() {
      String s = "<h1>WiFi Configuration Mode</h1><p><a href=\"/settings\">Wi-Fi Settings</a></p>";
      WEB_SERVER.send(200, "text/html", makePage("Access Point mode", s));
    });
 
  WEB_SERVER.begin();

}

// Button Functions
// ----------------
boolean triggerButtonEvent(String eventName)
{
  int BUTTON_COUNTER = 0;
  
  Serial.println("IFTT Button fired");  
  
  // Define the WiFi Client
  WiFiClient client;
  // Set the http Port
  const int httpPort = 80;

  // Make sure we can connect
  if (!client.connect(IFTTT_URL, httpPort)) {
    return false;
  }

  // We now create a URI for the request
  String url = "/trigger/" + String(eventName) + "/with/key/" + String(IFTTT_KEY);

  // Set some values for the JSON data depending on which event has been triggered
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String value_1 = "";
  String value_2 = "";
  String value_3 = "";
  if(eventName == IFTTT_EVENT){
    value_1 = String(BUTTON_COUNTER -1);
    value_2 = ipStr;
    value_3 = String(WiFi.gatewayIP());
  }
  else if(eventName == IFTTT_NOTIFICATION_EVENT){
    value_1 = ipStr;
    value_2 = WiFi.SSID();
  }


  // Build JSON data string
  String data = "";
  data = data + "\n" + "{\"value1\":\""+ value_1 +"\",\"value2\":\""+ value_2 +"\",\"value3\":\""+ value_3 + "\"}";

  // Post the button press to IFTTT
  if (client.connect(IFTTT_URL, httpPort)) {

    // Sent HTTP POST Request with JSON data
    client.println("POST "+ url +" HTTP/1.1");
    Serial.println("POST "+ url +" HTTP/1.1");
    client.println("Host: "+ String(IFTTT_URL));
    Serial.println("Host: "+ String(IFTTT_URL));
    client.println("User-Agent: Arduino/1.0");
    Serial.println("User-Agent: Arduino/1.0");
    client.print("Accept: *");
    Serial.print("Accept: *");
    client.print("/");
    Serial.print("/");
    client.println("*");
    Serial.println("*");
    client.print("Content-Length: ");
    Serial.print("Content-Length: ");
    client.println(data.length());
    Serial.println(data.length());
    client.println("Content-Type: application/json");
    Serial.println("Content-Type: application/json");
    client.println("Connection: close");
    Serial.println("Connection: close");
    client.println();
    Serial.println();
    client.println(data);
    Serial.println(data);
  }
}


// Debugging Functions - Not used
// -------------------
void wipeEEPROM()
{
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++)
    EEPROM.write(i, 0);

  EEPROM.end();
}


void loop() {
  if (configMode)
     WEB_SERVER.handleClient();   
}
