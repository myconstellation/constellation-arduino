#include <Constellation.h>

/* Arduino Wifi (ex: Wifi Shield) */
//#include <SPI.h>
//#include <WiFi.h>

/* Arduino Wifi101 (WiFi Shield 101 and MKR1000 board) */
//#include <SPI.h>
//#include <WiFi101.h>

/* Arduino Ethernet */
//#include <SPI.h>
//#include <Ethernet.h>

/* ESP8266 Wifi */
#include <ESP8266WiFi.h>

/* Wifi credentials */
char ssid[] = "YOUR_SSID";
char password[] = "YOUR_WIFI_PASSWORD";

/* Create the Constellation client */
Constellation<WiFiClient> constellation("IP_or_DNS_CONSTELLATION_SERVER", 8088, "YOUR_SENTINEL_NAME", "YOUR_PACKAGE_NAME", "YOUR_ACCESS_KEY");

void setup(void) {
  Serial.begin(115200);  delay(10);
  
  // Connecting to Wifi  
  Serial.print("Connecting to ");
  Serial.println(ssid);  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // For WiFi101, wait 10 seconds for connection!
  // delay(10000);
  
  Serial.println("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  
  // Show your sentinel name
  Serial.println(constellation.getSentinelName());
  
  // Getting setting value of your virtual package
  JsonObject& settings = constellation.getSettings();
  static int numberTest = settings["numberTest"].as<int>();
  static String stringTest = String(settings["stringTest"].as<char *>());
  
  // Expose a MessageCallback to reboot the device (ESP only)
  constellation.registerMessageCallback("Reboot",
    MessageCallbackDescriptor().setDescription("Reboot me !"),
    [](JsonObject& json) {
      #ifdef ESP8266
      constellation.writeInfo("Rebooting...");
      ESP.restart();
      #else
      constellation.writeInfo("Can't reboot ! It's not an ESP8266 ...");
      #endif
   });  
  
  // Expose another MessageCallback with response (saga)
  constellation.registerMessageCallback("Addition",
    MessageCallbackDescriptor().setDescription("Do addition on this tiny device").addParameter<int>("a").addParameter<int>("b").setReturnType<int>(),
    [](JsonObject& json, MessageContext ctx) {
      int a = json["Data"][0].as<int>();
      int b = json["Data"][1].as<int>();
      int result = a + b;
      constellation.writeInfo("Addition %d + %d = %d", a, b, result);
      if(ctx.isSaga) {
        constellation.writeInfo("Doing additoon for %s (sagaId: %s)", ctx.sender.friendlyName, ctx.sagaId);
        constellation.sendResponse(ctx, result);
      }
      else {
        constellation.writeInfo("No saga, no response !");
      }
   });
   
  // Expose a MessageCallback with optional parameter
  constellation.registerMessageCallback("SayHello",
    MessageCallbackDescriptor().setDescription("Say hello demo !").addOptionalParameter<const char*>("name", "boy", "Optional parameter with default value !"),
    [](JsonObject& json) {
      constellation.writeInfo("Hello %s", json["Data"].asString());
   });  

  // Describe a custom StateObject type  
  constellation.addStateObjectType("MyLuxData", TypeDescriptor().setDescription("MyLuxData demo").addProperty<int>("Broadband").addProperty<int>("IR").addProperty<int>("Lux"));

  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // WriteLog info
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());  
}

void loop(void) {
  delay(1000); // Best practice : don't wait here, add a counter or ArduinoThread
  constellation.writeInfo("I'm here !"); 

  // Dummy data
  uint16_t ir = 123, full = 465;
  uint32_t lux = 42;

  // Push a simple type on Constellation 
  constellation.pushStateObject("Lux", lux);

  // Push a complex object on Constellation with stringFormat
  constellation.pushStateObject("DemoLux", stringFormat("{ 'Lux':%d, 'Broadband':%d, 'IR':%d }", lux, full, ir));

  // Push a complex object on Constellation with JsonObject and custom type
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(5);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& myStateObject = jsonBuffer.createObject();
  myStateObject["Lux"] = lux;
  myStateObject["Broadband"] = full;
  myStateObject["IR"] = ir;
  constellation.pushStateObject("DemoLux2", myStateObject, "MyLuxData");

  // Process incoming message & StateObject updates
  constellation.loop();
}
