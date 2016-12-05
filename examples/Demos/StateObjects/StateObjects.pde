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

  // Describe your custom StateObject types  
  constellation.addStateObjectType("MyLuxData", TypeDescriptor().setDescription("MyLuxData demo").addProperty<int>("Broadband").addProperty<int>("IR").addProperty<int>("Lux"));
  
  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // ------ Request StateObjects ------

  // Example : print the all SO's value named "/intelcpu/0/load/0" and produced by the "HWMonitor" package (on all sentinels)
  JsonArray& cpus = constellation.requestStateObjects("*", "HWMonitor", "/intelcpu/0/load/0");
  for(int i=0; i < cpus.size(); i++) {
    // You can get the Value as float :
    //  float cpuValue = cpus[i]["Value"]["Value"].as<float>();
    // Note : by default the specifier %f or %g not work on tiny device, you must to convert the value as String (asString())
    // More info : http://stackoverflow.com/questions/905928/using-floats-with-sprintf-in-embedded-c
    constellation.writeInfo("CPU on %s is currently %s %s", cpus[i]["SentinelName"].asString(), cpus[i]["Value"]["Value"].asString(), cpus[i]["Value"]["Unit"].asString());
  }
  
  // ------ Subscribe to StateObjects ------
  
  // Method 1 : set a StateObject update callback and subscribe to SO
  /*constellation.setStateObjectUpdateCallback([] (JsonObject& so) {
    constellation.writeInfo("StateObject updated ! StateObject name = %s", so["Name"].asString()); 
  });
  constellation.subscribeToStateObjects("*", "HWMonitor", "/intelcpu/0/load/0");*/
  
  // Method 2 : register a StateObject link
  constellation.registerStateObjectLink("*", "HWMonitor", "/intelcpu/0/load/0", [](JsonObject& so) {
    constellation.writeInfo("CPU on %s is currently %s %s", so["SentinelName"].asString(), so["Value"]["Value"].asString(), so["Value"]["Unit"].asString());
  });
  
  
  // (Virtual) Package started
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());  
  
}

void loop(void) {
  delay(1000);  // Best practice : don't wait here, add a counter or ArduinoThread
  constellation.writeInfo("I'm here !"); 
  
  // Dummy data
  uint16_t ir = 123, full = 465;
  uint32_t lux = 42;

  // Push a simple type on Constellation 
  constellation.pushStateObject("DemoLux_A1", lux);  
  // Push a simple type on Constellation with lifetime of 20 seconds
  constellation.pushStateObject("DemoLux-A2", lux, 20);
  
  // Push a complex object on Constellation with stringFormat
  constellation.pushStateObject("DemoLux_B1", stringFormat("{ 'Lux':%d, 'Broadband':%d, 'IR':%d }", lux, full, ir));
  // The same with the custom type "MyLuxData" describe below
  constellation.pushStateObject("DemoLux_B2", stringFormat("{ 'Lux':%d, 'Broadband':%d, 'IR':%d }", lux, full, ir), "MyLuxData");
  // And with lifetime :
  constellation.pushStateObject("DemoLux_B3", stringFormat("{ 'Lux':%d, 'Broadband':%d, 'IR':%d }", lux, full, ir), "MyLuxData", 20);
  
  // Push a complex object on Constellation with JsonObject
  const int BUFFER_SIZE = JSON_OBJECT_SIZE(5);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& myStateObject = jsonBuffer.createObject();
  myStateObject["Lux"] = lux;
  myStateObject["Broadband"] = full;
  myStateObject["IR"] = ir;
  constellation.pushStateObject("DemoLux_C1", myStateObject);
  // The same with the custom type "MyLuxData" describe below
  constellation.pushStateObject("DemoLux_C2", myStateObject, "MyLuxData");
  // And with lifetime :
  constellation.pushStateObject("DemoLux_C3", myStateObject, "MyLuxData", 20);
  // You can also add metadata on your StateObject
  JsonObject& metadatas = jsonBuffer.createObject();
  metadatas["ChipId"] = ESP.getChipId();
  metadatas["Timestamp"] = millis();
  constellation.pushStateObject("DemoLux_C4", myStateObject, "MyStateObject", &metadatas, 20);

  // Process incoming message & StateObject updates
  constellation.loop();
}
