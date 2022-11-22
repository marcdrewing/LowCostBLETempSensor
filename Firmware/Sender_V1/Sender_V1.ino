
#include <BLEDevice.h>
#include <Adafruit_MAX31865.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

AsyncWebServer server(80);
Preferences preferences;

//const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";          //UUIDs werden über Browser eingestellt
//const char* CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

//const char *SERVICE_UUID = "bbbc115e-6774-4f7c-9c34-d9353a4464de";          //UUIDs werden über Browser eingestellt
//const char *CHARACTERISTIC_UUID = "83e4c644-8eca-405f-94f7-befe45f26db9";

//const char* SERVICE_UUID = "bbbc115e-6774-4f7c-9c34-d9353a4464fr";          //UUIDs werden über Browser eingestellt
//const char* CHARACTERISTIC_UUID = "83e4c644-8eca-405f-94f7-befe45f26dbg";

//const char* SERVICE_UUID = "8f430fd7-fa5a-4b79-a5ba-88b3bef80b1e";          //UUIDs werden über Browser eingestellt
//const char* CHARACTERISTIC_UUID = "801a69d7-697d-413b-9e25-281a79821cdd";

const char* PARAM_INPUT_1 = "input1";     //Speicher für Input aus dem HTML Formular zur Eingabe der UUIDs
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";
const char* PARAM_INPUT_4 = "input4";

int interval_before_configuration;
int interval_after_configuration;

bool connectedToBLE = false;
bool deviceConnected = false;
bool connectedToWifi = false;

//String test_variable = "test variable";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>BLE UUID Settings</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    BLE ID<br>
    Service UUID:<br> <input type="text" name="input1"><br>
    Characteristic UUID:<br> <input type="text" name="input2"><br>
    <input type="submit" value="Save"><br>
   </form><br>
   <form action="/get">
    Sending Data Interval<br>
    Interval during configuration_time in milliseconds:<br> <input type="text" name="input3"><br>
    Interval after configuration_time in milliseconds:<br> <input type="text" name="input4"><br>
    <input type="submit" value="Save"><br>
    <br>
   </form><br>
  <a href="exit"><button class=\"button\">Apply Configuration and Restart</button></a><br>
  <a href="now"><button class=\"button\">Saved Values</button></a><br>
  <br><br> If the transmitter does not have a connection with a receiver, it will restart every 20 seconds to reastablish the connection.<br>
  Once the website with IP 192.168.4.1 is opened, the transmitter will stop restarting, until the configuration is ended with the Button "Apply Configuration and Restart" <br>
  The Configuration will automatically end after 5 minutes to save energy. 
</body></html>)rawliteral";


Adafruit_MAX31865 thermo = Adafruit_MAX31865(5,23,19,18); // (cs, mosi, miso,clk) CS Pin hier eintragen ESP32 VSPI GPIO23MOSI GPIO19MISO GPIO18CLK GPIO5CS
#define RREF      430.0 //Referenzwiderstand auf der Platine
#define RNOMINAL  100.0 //Erwarteter Widerstand des PT100 Sensors bei 0°. Bei Verwendung eines PT1000 Sensors ändern in 1000.0

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Connected");
    deviceConnected = true;
    connectedToBLE = true;
  };
  void onDisconnect(BLEServer* pServer) {
    Serial.println("Disconnected");
    deviceConnected = false;
    ESP.restart();
    
  }
};

void setup() {
  setCpuFrequencyMhz(80);
  delay(1000);
  Serial.begin(115200);
  pinMode(32, INPUT_PULLUP);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);

  
  

  preferences.begin("UUID", false); 

  String service_UUID_preference = preferences.getString("service", "0"); //Falls noch kein Passwort für den AccessPoint gespeichert wurde, wird eine 0 zurück gegeben
  Serial.println("service_UUID_preference");
  Serial.println(service_UUID_preference);
  if(service_UUID_preference == "0" || service_UUID_preference == ""){                                        //Wenn kein Passwort gespeichert ist, wird das standard Passwprt verwendet
    SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  } else{
    SERVICE_UUID = service_UUID_preference.c_str();
  }

  String characteristic_UUID_preference = preferences.getString("characteristic", "0"); //Falls noch kein Passwort für den AccessPoint gespeichert wurde, wird eine 0 zurück gegeben
  Serial.println("characteristic_UUID_preference");
  Serial.println(characteristic_UUID_preference);
  if(characteristic_UUID_preference == "0" || characteristic_UUID_preference == ""){                                        //Wenn kein Passwort gespeichert ist, wird das standard Passwprt verwendet
    CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  } else{
    CHARACTERISTIC_UUID = characteristic_UUID_preference.c_str();
  }

  interval_before_configuration = preferences.getString("intervalb", "500").toInt();
  interval_after_configuration = preferences.getString("intervala", "30000").toInt();
  
  preferences.end();

  if(digitalRead(32)==LOW){       //Activate Wifi only if the Configuration Switch is turned on
    
    login();
  }
  
  Serial.println("Characteristic UUID:");
  Serial.println(CHARACTERISTIC_UUID);
  Serial.println("Service UUID:");
  Serial.println(SERVICE_UUID);

  thermo.begin(MAX31865_2WIRE);  //Entsprechend des Sensors zu 2 oder 4 Wire ändern
  SPI.begin(SCK, MISO, MOSI, SS);
  
  delay(1000);
  Serial.println("1");
  BLEDevice::init("TemperatureSensor1");
  Serial.println("2");
  BLEServer* tserver = BLEDevice::createServer();
  tserver->setCallbacks(new MyServerCallbacks());
  Serial.println("3");
  BLEService* tservice = tserver->createService(SERVICE_UUID);
  Serial.println("4");
  BLECharacteristic* temp = tservice->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  Serial.println("5");
  temp->setValue("Messung wird gestartet");
  Serial.println("6");
  tservice->start();
  Serial.println("7");
  BLEAdvertising *tAdvertising = tserver->getAdvertising();
  Serial.println("8");
  tAdvertising->addServiceUUID(SERVICE_UUID);
  Serial.println("9");
  tAdvertising->setScanResponse(true);
  Serial.println("10");
  tAdvertising->start();
  Serial.println("11");

  digitalWrite(33, HIGH);
  delay(500);
  digitalWrite(33, LOW);
  delay(1000);

  while(1){

    uint16_t rtd = thermo.readRTD();

    Serial.print("RTD value: "); Serial.println(rtd);
    float ratio = rtd;
    ratio /= 32768;
    Serial.print("Ratio = "); Serial.println(ratio,8);
    Serial.print("Resistance = "); Serial.println(RREF*ratio,8);
    Serial.print("Temperature = "); Serial.println(thermo.temperature(RNOMINAL, RREF));

//    uint8_t fault = thermo.readFault();
//    if (fault) {
//      Serial.print("Fault 0x"); Serial.println(fault, HEX);
//      if (fault & MAX31865_FAULT_HIGHTHRESH) {
//        Serial.println("RTD High Threshold"); 
//      }
//      if (fault & MAX31865_FAULT_LOWTHRESH) {
//        Serial.println("RTD Low Threshold"); 
//      }
//      if (fault & MAX31865_FAULT_REFINLOW) {
//        Serial.println("REFIN- > 0.85 x Bias"); 
//      }
//      if (fault & MAX31865_FAULT_REFINHIGH) {
//        Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
//      }
//      if (fault & MAX31865_FAULT_RTDINLOW) {
//        Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
//      }
//      if (fault & MAX31865_FAULT_OVUV) {
//        Serial.println("Under/Over voltage"); 
//      }
//      thermo.clearFault();
//    }

    
    
    int rounded = (thermo.temperature(RNOMINAL, RREF))*100;
    String helpstring = String(rounded);
    Serial.println(helpstring);
    std::string myStringForUnit16((char*)&helpstring, 5);
    temp->setValue(myStringForUnit16);
    delay(10);
    digitalWrite(33, HIGH);
    delay(20);
    digitalWrite(33, LOW);

//    if(millis()>30000){                      //Wlan schaltet sich nach 5 minuten (300000 millisekunden) selbstständig aus
//      connectedToWifi = false;
//      server.end();
//      WiFi.mode(WIFI_OFF);
//    }

//    if(millis()>60000){Serial.println("Time > 20 seconds");}
//    Serial.println(connectedToBLE);
//    Serial.println(connectedToWifi);
//    if(millis()>20000 && connectedToBLE == false && connectedToWifi == false){     //Startet den ESP32 neu, um nach Empfängern zu suchen
//      Serial.println("Restart after Timeout");
//      ESP.restart();
//    }

    if(millis()>10000 && connectedToBLE == false && digitalRead(32) == true){     //Startet den ESP32 neu, um nach Empfängern zu suchen
      Serial.println("Restart after Timeout");
      ESP.restart();
    }

    if(millis()<300000){
      delay(interval_before_configuration);
    }
    if(millis()>=300000){
      delay(interval_after_configuration);
    }
    
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void login() {
  Serial.println("loggin in");
  const char *ssidAP = "BLE_Temperatur_Sensor1";
  const char *passwordAP = "yourPassword"; 
      
  
  
  Serial.println("AP SSID:");
  Serial.println(ssidAP);
  Serial.println("AP Passwort:");
  Serial.println(passwordAP);
  
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAP, passwordAP);
  Serial.print("Login Credentials");
  Serial.print(ssidAP);
  Serial.print(passwordAP);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  Serial.println();
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
    connectedToWifi = true;
  });

  server.on("/now", HTTP_GET, [](AsyncWebServerRequest *request){
    String var1 = String(SERVICE_UUID);
    String var2 = String(CHARACTERISTIC_UUID);
    String var3 = String(interval_before_configuration);
    String var4 = String(interval_after_configuration);
    request->send(200, "text/html", "Service UUID: " + var1 + "<br> Characteristic UUID: " + var2 + "<br> Time interval of sending data packages during configuration: " + var3 +"ms"+ "<br> Time interval of sending data packages after configuration: " + var4 +"ms"+ "<br><a href=\"/\">Go Back</a>");
  });

  server.on("/exit", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "Konfiguration wird beendet");
    server.end();
    connectedToWifi = false;
    WiFi.mode(WIFI_OFF);
    //digitalWrite(2, HIGH);
    return 0;
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    preferences.begin("UUID", false);
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputMessage = inputMessage+request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_1;
      String serviceSpeicher2 = request->getParam(PARAM_INPUT_1)->value();              //Auslesen der Eingabe auf der Website
      String characteristicSpeicher2 = request->getParam(PARAM_INPUT_2)->value();
      preferences.putString("service", serviceSpeicher2);                               //Speichern der eingegebenen UUID im Flash Speicher des Microcontrollers
      preferences.putString("characteristic", characteristicSpeicher2);
      
      String saveService = preferences.getString("service", "nichts enthalten");                //Auslesen der Preferenzen um zu schauen ob der eingegebene Wert gespeichert wurde         
      String saveCharacteristic = preferences.getString("characteristic", "nichts enthalten");
      Serial.println(saveService);
      Serial.println(saveCharacteristic);
      
      //SERVICE_UUID = serviceSpeicher2.c_str();            //Überschreiben der UUID Speicher mit dem neuen Wert
      //CHARACTERISTIC_UUID = characteristicSpeicher2.c_str();
    }
    if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputMessage = inputMessage+request->getParam(PARAM_INPUT_4)->value();
      inputParam = PARAM_INPUT_3;
      String interval_before_config2 = request->getParam(PARAM_INPUT_3)->value();              //Auslesen der Eingabe auf der Website
      String interval_after_config2 = request->getParam(PARAM_INPUT_4)->value();
      preferences.putString("intervalb", interval_before_config2);                               //Speichern der eingegebenen UUID im Flash Speicher des Microcontrollers
      preferences.putString("intervala", interval_after_config2);
      
      String testintervalb = preferences.getString("intervalb", "nichts enthalten");                //Auslesen der Preferenzen um zu schauen ob der eingegebene Wert gespeichert wurde         
      String testintervala = preferences.getString("intervala", "nichts enthalten");
      Serial.println(testintervalb); 
      Serial.println(testintervala);
      
      //SERVICE_UUID = serviceSpeicher2.c_str();            //Überschreiben der UUID Speicher mit dem neuen Wert
      //CHARACTERISTIC_UUID = characteristicSpeicher2.c_str();
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    preferences.end();
    Serial.println(inputMessage);
//    Serial.println("SSID:");
//    Serial.println(ssid);
//    Serial.println("Passwort:");
//    Serial.println(password);

    //login(ssid2,password2);
    
    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
    
  });
  server.onNotFound(notFound);
  server.begin();
}
