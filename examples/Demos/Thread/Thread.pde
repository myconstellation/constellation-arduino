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

#include <Thread.h>
#include <ThreadController.h>
ThreadController thrController = ThreadController();
Thread thrDoMeasure1 = Thread();
Thread thrDoMeasure2 = Thread();

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
  //delay(10000);

  Serial.println("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  // Show your sentinel name
  Serial.println(constellation.getSentinelName());

  // Getting setting value of your virtual package
  JsonObject& settings = constellation.getSettings();
  static int numberTest = settings["numberTest"].as<int>();
  static String stringTest = String(settings["stringTest"].asString());

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

  // Expose a MessageCallback with one parameter
  constellation.registerMessageCallback("SayHello",
    MessageCallbackDescriptor().setDescription("Say hello demo !").addParameter<const char*>("name"),
    [](JsonObject& json) {
      constellation.writeInfo("Hello %s", json["Data"].asString());
   });

  // Write info & Push simple StateObject every 10 seconds
  thrDoMeasure1.onRun([]() {
    // Write info
    constellation.writeInfo("I'm here !");
    // Push a simple type on Constellation
    uint32_t lux = 42; // Dummy value !
    constellation.pushStateObject("Lux", lux);
  });
  thrDoMeasure1.setInterval(10000);
  thrController.add(&thrDoMeasure1);

  // Push complex StateObject every second
  thrDoMeasure2.onRun([]() {
    // Push a complex object on Constellation with JsonObject and custom type
    const int BUFFER_SIZE = JSON_OBJECT_SIZE(5);
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
    JsonObject& myStateObject = jsonBuffer.createObject();
    myStateObject["Lux"] = 42; // Dummy value !
    myStateObject["Broadband"] = 123; // Dummy value !
    myStateObject["IR"] = 6; // Dummy value !
    constellation.pushStateObject("Lux2", myStateObject, "MyLuxData");
  });
  thrDoMeasure2.setInterval(1000);
  thrController.add(&thrDoMeasure2);

  // Describe a custom StateObject type
  constellation.addStateObjectType("MyLuxData", TypeDescriptor().setDescription("MyLuxData demo").addProperty<int>("Broadband").addProperty<int>("IR").addProperty<int>("Lux"));	

  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // WriteLog info
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());
}

void loop(void) {
  // Process incoming message & StateObject updates
  constellation.loop();
  // Process threads
  thrController.run();
}
