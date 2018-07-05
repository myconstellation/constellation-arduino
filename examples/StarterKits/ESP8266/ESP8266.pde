#include <Constellation.h>

// ESP8266 Wifi
#include <ESP8266WiFi.h>
char ssid[] = "YOUR_SSID";
char password[] = "YOUR_WIFI_PASSWORD";

// Constellation client (use WiFiClientSecure for SSL)
Constellation<WiFiClient> constellation("IP_or_DNS_CONSTELLATION_SERVER", 8088, "YOUR_SENTINEL_NAME", "YOUR_PACKAGE_NAME", "YOUR_ACCESS_KEY");

void setup(void) {
  Serial.begin(115200);  delay(10);
  
  // Set Wifi mode
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }
  
  // Connecting to Wifi  
  Serial.print("Connecting to ");
  Serial.println(ssid);  
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  
  // Get package settings (see BasicDemo example)
  // Register StateObjectLinks (see StateObjectsDemo example)
  // Register MessageCallbacks (see MessageCallbackDemo example)
  // Describe SO & MC types (see StateObjectsDemo and MessageCallbackDemo example)
  
  // Declare the package descriptor
  constellation.declarePackageDescriptor();

  // WriteLog info
  constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());  
}

void loop(void) {
  constellation.loop();
}