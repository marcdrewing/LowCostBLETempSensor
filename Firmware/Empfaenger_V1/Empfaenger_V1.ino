#include "BLEDevice.h"
//#include "BLEScan.h"
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

const char *ssidAP = "BLE_Temperatur_Receiver1";    //Ändern: Name des Access Points
const char *passwordAP = "123456789";    

//const char* SERVICE_UUID = "8f430fd7-fa5a-4b79-a5ba-88b3bef80b1e";          //UUIDs werden über Browser eingestellt
//const char* CHARACTERISTIC_UUID = "801a69d7-697d-413b-9e25-281a79821cdd";

const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";          //UUIDs werden über Browser eingestellt
const char* CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"; 

std::string servicestring = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
std::string charstring = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

//const char* ServicePointer = SERVICE_UUID;
//const char* CharPointer = CHARACTERISTIC_UUID;

//static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");            //Service UUID nach der gesucht wird
//static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

//static BLEUUID serviceUUID(SERVICE_UUID);            //Service UUID nach der gesucht wird
//static BLEUUID charUUID(CHARACTERISTIC_UUID);

//static BLEUUID* servicePointer = &serviceUUID; 
//static BLEUUID* charPointer = &charUUID; 

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;            //Pointer mit dem Typ "BLERemoteCharacteristic" 
static BLEAdvertisedDevice* myDevice;                           

Adafruit_MCP4725 dac;
AsyncWebServer server(80);
Preferences preferences;

#define DAC_RESOLUTION    (8)
int maxtemp = 200;                    //Werte werden später mit den Werten aus dem Speicher ersetzt
int mintemp = 0;

const char* PARAM_INPUT_1 = "input1";     //Speicher für Input aus dem HTML Formular zur Eingabe der UUIDs
const char* PARAM_INPUT_2 = "input2";
const char* PARAM_INPUT_3 = "input3";
const char* PARAM_INPUT_4 = "input4";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>BLE UUID Settings</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    BLE ID<br>
    Service UUID:<br> <input type="text" name="input1"><br>
    Characteristic UUID:<br> <input type="text" name="input2"><br>
    <input type="submit" value="Anwenden"><br>
  </form><br>
  <form action="/get">
    Messbereich fuer analoge Ausgabe<br>
    Mintemp:<br> <input type="text" name="input3"><br>
    Maxtemp:<br> <input type="text" name="input4"><br>
    <input type="submit" value="Anwenden"><br>
  </form><br>
  <a href="now"><button class=\"button\">aktuelle UUID und Messbereich</button></a><br>
</body></html>)rawliteral";


static void notifyCallback(                                     //Methode notifyCallback, an die Methode werden übergeben: Pointer pBLERemoteCharacteristic, Pointer pData, Länge, Bool  
  BLERemoteCharacteristic* pBLERemoteCharacteristic,            
  uint8_t* pData,
  size_t length,                                                   //size_t quasi wie int, Unterschiede aber hier unwichtig
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());    //
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    static BLEUUID serviceUUID(servicestring);            //Service UUID nach der gesucht wird
    static BLEUUID charUUID(charstring);
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
    delay(2000);
    ESP.restart();
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);
  pinMode(32, INPUT_PULLUP);
  delay(50);
  dac.begin(0x60);
  delay(500);
  dac.setVoltage(int(0), false);
  delay(500);

  preferences.begin("UUID", false); 

  String service_UUID_preference = preferences.getString("service", "0"); //Falls noch kein Passwort für den AccessPoint gespeichert wurde, wird eine 0 zurück gegeben
  Serial.println("service_UUID_preference");
  Serial.println(service_UUID_preference);
  if(service_UUID_preference == "0" || service_UUID_preference == ""){                                        //Wenn kein Passwort gespeichert ist, wird das standard Passwprt verwendet
    //const char* speicher1 = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    //SERVICE_UUID = speicher1;
    //Serial.println("Daten nicht aus Speicher");
    SERVICE_UUID = "246bad91-aeaa-40c5-9875-74bf16dbd612";
  } else{
    const char* NEW_SERVICE_UUID = service_UUID_preference.c_str();
    //ServicePointer = NEW_SERVICE_UUID;
    servicestring = service_UUID_preference.c_str();
    //SERVICE_UUID = teststring;
    //Serial.println(SERVICE_UUID);
    //Serial.println(*ServicePointer);
    Serial.println(servicestring.c_str());
    //SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    //Serial.println("Daten aus Speicher");
    //SERVICE_UUID = "246bad91-aeaa-40c5-9875-74bf16dbd612";
  }

  String characteristic_UUID_preference = preferences.getString("characteristic", "0"); //Falls noch kein Passwort für den AccessPoint gespeichert wurde, wird eine 0 zurück gegeben
  Serial.println("characteristic_UUID_preference");
  Serial.println(characteristic_UUID_preference);
  if(characteristic_UUID_preference == "0" || characteristic_UUID_preference == ""){                                        //Wenn kein Passwort gespeichert ist, wird das standard Passwprt verwendet
    //CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    //Serial.println("Daten nicht aus Speicher");
    CHARACTERISTIC_UUID = "8a9b3c9f-0c20-449b-860a-82019368af66";
  } else{
    charstring = characteristic_UUID_preference.c_str();
    Serial.println(charstring.c_str());
    //CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    //Serial.println("Daten aus Speicher");
    //CHARACTERISTIC_UUID = "8a9b3c9f-0c20-449b-860a-82019368af66";
  }

  String mintemp_preference = preferences.getString("mintemp", "NULL");
  if(mintemp_preference == "NULL" || mintemp_preference == ""){
    mintemp = 0;
  }
  else{
    mintemp = mintemp_preference.toInt();
  }

  String maxtemp_preference = preferences.getString("maxtemp", "NULL");
  if(maxtemp_preference == "NULL" || maxtemp_preference == ""){
    maxtemp = 200;
  }
  else{
    maxtemp = maxtemp_preference.toInt();
  }

  Serial.println("Mintemp");
  Serial.println(mintemp);
  Serial.println("Maxtemp");
  Serial.println(maxtemp);

  preferences.end();

  //SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  //CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  //SERVICE_UUID = "246bad91-aeaa-40c5-9875-74bf16dbd612";          //UUIDs werden über Browser eingestellt
  //CHARACTERISTIC_UUID = "8a9b3c9f-0c20-449b-860a-82019368af66";

  //if(digitalRead(32) == LOW){   //Erst verwenden wenn tatsächlich Schalter angelötet ist
  //  login();
  //}

  login();
  

  
  
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}



bool connectToServer() {

    Serial.println(SERVICE_UUID);
    
    static BLEUUID serviceUUID(servicestring);            //Service UUID nach der gesucht wird
    static BLEUUID charUUID(charstring);

    Serial.println(serviceUUID.toString().c_str());
  
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      //BLEUUID serviceSpeicher = *servicePointer;
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      //BLEUUID charSpeicher = *charPointer;
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    while(1){
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      String value2 = value.c_str();
      int value3 = value2.toInt();
      float value4 = float(value3);
      float value5 = value4/100;
      
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
      //Serial.println(value2);
      //Serial.println(value3);
      //Serial.println(value4);
      Serial.print(value5);
      Serial.println("°C");
      Serial.println("!Messung mit nur 2 Leitungen resultiert in einem Offset von mehreren °C!");
      float tempDif = maxtemp-mintemp; 

      Serial.println("Maxtemp:");
      Serial.println(maxtemp);
      Serial.println("Mintemp:");
      Serial.println(mintemp);
 
      
      float outputVoltage = (((value5)-float(mintemp))*4095.00)/(tempDif);
      if(outputVoltage>=4095.00){
        outputVoltage=4095.00;
      }
      Serial.println(outputVoltage);

      delay(500);
      dac.setVoltage(int(outputVoltage), false);
      delay(500);
      
      digitalWrite(33, HIGH);
      delay(200);
      digitalWrite(33, LOW);
      

      

    }}

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */





// This is the Arduino main loop function.
void loop() {
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }
  delay(500); // Delay a second between loops.
  if (millis()>=10000 && digitalRead(32) == true){   //Wenn kein Server gefunden wurde (andernfalls würde das Programm die while(1) Schleife nicht mehr verlassen
    ESP.restart();        // dann wird der Empfänger neu gestartet
  }
} // End of loop

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void login() {
  Serial.println("loggin in");
 
  Serial.println("AP SSID:");
  Serial.println(ssidAP);
  Serial.println("AP Passwort:");
  Serial.println(passwordAP);
  
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidAP, passwordAP);
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
  });

  server.on("/now", HTTP_GET, [](AsyncWebServerRequest *request){
    String var1 = servicestring.c_str();
    String var2 = charstring.c_str();
    String var3 = String(mintemp);
    String var4 = String(maxtemp);
    Serial.println(var1);
    Serial.println(var2);
    request->send(200, "text/html", "Service UUID:            " + var1 + "<br> Characteristic UUID:     " + var2 + "<br>Mintemp: " + var3+ "<br>Maxtemp: " + var4 + "<br><a href=\"/\">Zurueck</a>");
  });

  server.on("/exit", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "Konfiguration wird beendet");
    preferences.end();
    server.end();
    WiFi.mode(WIFI_OFF);
    digitalWrite(2, HIGH);
    return 0;
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    Serial.println("Sent form");
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

      preferences.end();
      
      
    }
    if (request->hasParam(PARAM_INPUT_3)) {
      Serial.println("Received Form");
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputMessage = inputMessage+request->getParam(PARAM_INPUT_4)->value();
      inputParam = PARAM_INPUT_3;
      String mintempSpeicher2 = request->getParam(PARAM_INPUT_3)->value();              //Auslesen der Eingabe auf der Website
      String maxtempSpeicher2 = request->getParam(PARAM_INPUT_4)->value();
      Serial.println("Read Values");
      preferences.putString("maxtemp", maxtempSpeicher2);                               //Speichern der eingegebenen UUID im Flash Speicher des Microcontrollers
      preferences.putString("mintemp", mintempSpeicher2);
      Serial.println("Wrote Values to Preferences");
      String savemaxtemp = preferences.getString("maxtemp", "nichts enthalten");                //Auslesen der Preferenzen um zu schauen ob der eingegebene Wert gespeichert wurde         
      String savemintemp = preferences.getString("mintemp", "nichts enthalten");
      Serial.println("Test Read Values from Preferences");
      Serial.println(savemaxtemp);
      Serial.println(savemintemp);

      preferences.end();
      delay(500);
      //ESP.restart();
    }
    else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    Serial.println(inputMessage);
//    Serial.println("SSID:");
//    Serial.println(ssid);
//    Serial.println("Passwort:");
//    Serial.println(password);

    request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" 
                                     + inputParam + ") with value: " + inputMessage +
                                     "<br><a href=\"/\">Return to Home Page</a>");
    delay(500);
    ESP.restart();
  });
  server.onNotFound(notFound);
  server.begin();
}
