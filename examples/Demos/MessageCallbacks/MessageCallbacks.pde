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

  // --------- Method 1 : Receive all incoming in function ---------
  
  // Set callback for all incoming messages
  constellation.setMessageReceiveCallback([](JsonObject& json) { 
    constellation.writeInfo("Message receive ! Message key = %s", json["Key"].asString());
  });  
  
  // Set callback for all incoming messages with MessageContext
  constellation.setMessageReceiveCallback([](JsonObject& json, MessageContext ctx) { 
    constellation.writeInfo("Message receive from %s. Message key = %s", ctx.sender.friendlyName, json["Key"].asString());
  });  
  
  // Subscribe to message
  constellation.subscribeToMessage();
  

  // --------- Method 2 : regiter MessageCallback on specific key ---------

  // Expose a simple MessageCallback (without MessageCallbackDescriptor this MC is hidden)
  constellation.registerMessageCallback("HelloWorld",
    [](JsonObject& json) {
      constellation.writeInfo("Hello Constellation !");
   });  
  
  // Expose a MessageCallback with descriptor :
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
   
  // Expose a MessageCallback with multiples and optionals parameters :
  constellation.registerMessageCallback("SayHello",
    MessageCallbackDescriptor().setDescription("Say hello !").addParameter<const char*>("FirstName").addOptionalParameter<const char*>("LastName", "Default Value here"),
    [](JsonObject& json) {
      if(json["Data"].is<JsonArray&>()) {
        // If there is more than one parameter, you get a JSON array with the MC parameters
        constellation.writeInfo("Hello %s %s", json["Data"][0].asString(), json["Data"][1].asString()); 
      }
      else {
        // Otherwise, you get the single parameter in'Data' (the 'LastName' is optional in this MessageCallbackDescriptor, so you can get one or two parameters)
        constellation.writeInfo("Hello %s", json["Data"].asString()); 
      }
   });
   
  // Expose a MessageCallback with complex parameter :
  constellation.registerMessageCallback("SayHello2",
    MessageCallbackDescriptor().setDescription("Say hello with complex object!").addParameter("User", "SampleUserData"),
    [](JsonObject& json) {
      constellation.writeInfo("Hello %s %s", json["Data"]["FirstName"].asString(), json["Data"]["LastName"].asString()); 
  });
  // and describe the complex parameter "SampleUserData"  
  constellation.addMessageCallbackType("SampleUserData", TypeDescriptor().setDescription("This is a sample user data").addProperty<const char*>("FirstName", "The first name").addProperty<const char*>("LastName", "The last name")); 
  
  // Expose MessageCallback with response (saga)
  constellation.registerMessageCallback("Addition",
    MessageCallbackDescriptor().setDescription("Do addition on this tiny device").addParameter<int>("a").addParameter<int>("b").setReturnType<int>(),
    [](JsonObject& json, MessageContext ctx) {
      int a = json["Data"][0].as<int>();
      int b = json["Data"][1].as<int>();
      int result = a + b;
      constellation.writeInfo("Addition %d + %d = %d", a, b, result);
      if(ctx.isSaga) {
        constellation.writeInfo("Doing additoon for %s (sagaId: %s)", ctx.sender.friendlyName, ctx.sagaId);
        // Return the result :
        constellation.sendResponse(ctx, result);
      }
      else {
        constellation.writeInfo("No saga, no response !");
      }
   });   
  
  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // Subscribe to message group
  constellation.subscribeToGroup("Demo");

  // WriteLog info
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());  
  
  // --------- Send message ---------
  
  // Send message "LockWorkStation" to all "WindowsControl" package in your Constellation
  constellation.sendMessage(Package, "WindowsControl", "LockWorkStation", "{}");
  
  // Send message "LockWorkStation" to the "WindowsControl" package on the "MY-PC" sentinel
  constellation.sendMessage(Package, "MY-PC/WindowsControl", "LockWorkStation", "{}");
  
  // Send message "Ping" to all packages "NetworkTools" with 1 parameter
  constellation.sendMessage(Package, "NetworkTools", "Ping", "[ '192.168.0.10' ]");

  // SendMessage auto-format the message data with variatic parameters :
  const char* ip = "192.168.0.10";
  constellation.sendMessage(Package, "NetworkTools", "Ping", "[ '%s' ]", ip);
  
  // Send message "SayHello" to all packages in the "Demo" group with 2 parameters :
  constellation.sendMessage(Group, "Demo", "SayHello", "[ 'Sebastien', 'Warin' ]");
  
  // Send message "SayHello2" to all packages in the "Demo2" group with 1 complex parameter  :
  constellation.sendMessage(Group, "Demo", "SayHello", "{ 'FirstName':'Sebastien', 'LastName':'Warin' }");
  
  // Send message in a saga and attach a callback for response :
  constellation.sendMessageWithSaga([](JsonObject& json) {
    constellation.writeInfo("Ping response in %s ms", json["Data"].asString()); 
  }, Package, "NetworkTools", "Ping", "[ '%s' ]", ip);
}

void loop(void) {
  delay(1000); // Best practice : don't wait here, add a counter or ArduinoThread
  constellation.writeInfo("I'm here !"); 
  
  // Process incoming message & StateObject updates
  constellation.loop();
}
