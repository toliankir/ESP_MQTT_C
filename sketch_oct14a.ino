#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include "DHTesp.h"

const char* mqtt_server = "m13.cloudmqtt.com";
const int mqtt_port = 17447;
const char* mqtt_username = "fjwkxsvo";
const char* mqtt_password = "0uLbMzS4c4mr";
long lastMsg = 0;
char devName[50];
int pin4State = 0;

ESP8266WebServer server(80);
DHTesp dht;
WiFiClient wifiClient;

void callback(char* topic, byte* payload, unsigned int length) {
//  Serial.print("Message arrived [");
//  Serial.print(topic);
//  Serial.print("] ");
//  Serial.print(length);
//  for (int i=0;i<length;i++) {
//    Serial.print((char)payload[i]);
//  }
if (String(topic).indexOf("relay") != -1 && length == 1) {
  if (topic[String(topic).length()-1]=='0' && payload[0]=='0') {
    pin4State = 0;
    }
  if (topic[String(topic).length()-1]=='0' && payload[0]=='1') {
    pin4State = 1;
    }
  }  
}

PubSubClient mqttClient(wifiClient);

struct { 
  char ssid[32] = "TP-LINK_home2";
  char password[32] = "3Tolian2";
} wifiData;

void apRoot() {
  int n = WiFi.scanNetworks();
  String msg = "<html><body><ul>";
  for (int i = 0; i < n; i++) {
    msg += "<li>";
    msg += WiFi.SSID(i);
    msg += "</li>";
    }
  msg += "</ul><form action=\"/save\" method=\"POST\">";
  msg += "<p><input type=text name=\"ssid\" id=\"ssid\"></p>";
  msg += "<p><input type=text name=\"password\"></p>";
  msg += "<input type=\"Submit\"></form></body>";
  msg += "<script>";
  msg += "const ssidInput = document.querySelector('#ssid');";
  msg += "document.querySelectorAll('li').forEach(el => {";
  msg += "el.addEventListener('click', (ev) => {";
  msg += "ssidInput.value = ev.target.innerText;";
  msg += "});});";
  msg += "</script></html>";
  server.send(200, "text/html", msg);
  }

void apSaveAP(){
  server.send(200, "text/html", "AP data saved.");
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  ssid.toCharArray(wifiData.ssid, ssid.length());
  password.toCharArray(wifiData.password, password.length());
  EEPROM.put(0, wifiData);
  EEPROM.commit();
  Serial.print(wifiData.ssid);
  Serial.print(" ");
  Serial.println(wifiData.password);
  }

void initAP(){
  Serial.print("Starting access point (");
  Serial.print(devName);
  Serial.println(")...");
  WiFi.softAP(devName);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP adress:");
  Serial.println(myIP);
  server.on("/", apRoot);
  server.on("/save", apSaveAP);
  server.begin();
  Serial.println("HTTP Server started.");
  }

bool initWifiClient(){
  Serial.print("Connecting to ");
  Serial.println(wifiData.ssid);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  int count = 0;
  WiFi.begin(wifiData.ssid, wifiData.password);
   while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    count++;
    if (count > 10) {
      return false;
      }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
  }


void initMqtt(){
    if (mqttClient.connect(devName, mqtt_username, mqtt_password)) {
    Serial.println("MQTT connect");
    String sub = String(devName) + "/set/#";
    Serial.print("Subscribe to: ");
    Serial.println(sub);
    mqttClient.subscribe(sub.c_str());
  }
}

void setup() {
  pinMode(4, OUTPUT);

  // put your setup code here, to run once:
  //  EEPROM.begin(512);
  //  EEPROM.get(0, wifiData);
  sprintf(devName, "ESP_%d", system_get_chip_id());
  delay(1000);
  Serial.begin(115200);
  if (initWifiClient()) {
      dht.setup(2, DHTesp::DHT11);
      mqttClient.setServer(mqtt_server, mqtt_port);
      mqttClient.setCallback(callback);
      initMqtt();
    } else {
      Serial.println("WiFi connection error.");  
      initAP();
    }
}


void loop() {
  digitalWrite(4, pin4State);
  server.handleClient();
  mqttClient.loop();
  long now = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    initWifiClient();
    }
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    initMqtt();
    }

  if (now - lastMsg > 10000 && WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    lastMsg = now;

    int humidity = dht.getHumidity();
    int temperature = dht.getTemperature();
    String msg = "{\"humidity\": " + String(humidity) +", \"temperature\": "+ String(temperature) +" }";
    String topic = String(devName) + "/sensor";
    mqttClient.publish(topic.c_str() , msg.c_str());
    
    msg = "{\"state\": " + String(pin4State) +" }";
    topic = String(devName) + "/relay0";
    mqttClient.publish(topic.c_str() , msg.c_str());
    }

}
