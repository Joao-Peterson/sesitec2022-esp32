#include <ModbusMaster.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define RE 5
#define DE 18
const char *ssid = "IFC-LAB-B204";
const char *pass = "7wwxcb3a";
const char *spin = "/-\\|";
const char *statuses[] = {
  "WL_IDLE_STATUS",
  "WL_NO_SSID_AVAIL",
  "WL_SCAN_COMPLETED",
  "WL_CONNECTED",
  "WL_CONNECT_FAILED",
  "WL_CONNECTION_LOST",
  "WL_DISCONNECTED"
};

float current = 0.0;
float frequency = 0.0;

void setup() {
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  digitalWrite(RE, 0);
  digitalWrite(DE, 0);
  
  Serial.begin(115200);

  xTaskCreate(
    webserver_task,
    "webserver_task",
    4086,
    NULL,
    1,
    NULL
  );

  xTaskCreate(
    modbus_task,
    "modbus_task",
    4086,
    NULL,
    1,
    NULL
  );
}

void loop() {
  vTaskDelay(1);
}

WebServer server(5050);
void webserver_task(void *data){
  WiFi.begin(ssid, pass);
  Serial.println("Connecting to wifi...");
  Serial.print(spin[0]);
  
  bool waitWifi = true;
  bool check = true;
  uint8_t spin_count = 0;
  
  while(waitWifi){
    if(check){
      int st = WiFi.status();
      switch(st){
        case WL_NO_SSID_AVAIL:
          Serial.println();
          Serial.println("Error: The wifi ssid specified was not found");
          check = false;
          break;

        default:
        case WL_IDLE_STATUS:
          Serial.print("\r[");
          Serial.print(spin[spin_count%4]);
          Serial.print("] Status: [");
          Serial.print(statuses[st]);
          Serial.print("]");
          spin_count++;
          break;
          
        case WL_CONNECT_FAILED:
          Serial.println();
          Serial.println("Error: couldn't connect to wifi");
          check = false;
          break;
  
        case WL_CONNECTED:
          Serial.println();
          Serial.println("Success: wifi is connected");
          Serial.print("IP: ");
          Serial.println(WiFi.localIP());
          waitWifi = false;
          break;
      }  
    }
    
    delay(200);
  }

  Serial.println("Starting webserver");
  server.on("/esp", HTTP_GET, esp_get);
  server.onNotFound(not_found);
  server.begin();

  while(1){
    server.handleClient();
    vTaskDelay(1);
  }
}

void modbus_task(void *data){
  ModbusMaster mod;
  Serial2.begin(19200, SERIAL_8N2);
  mod.begin(5, Serial2);
  mod.preTransmission(pre);
  mod.postTransmission(post);
  Serial.println("Starting modbus com");
  
  while(1){
    uint8_t res = mod.readHoldingRegisters(0x0003, 3);
  
    if(res == mod.ku8MBSuccess){
      frequency = mod.getResponseBuffer(0x02);
      current = mod.getResponseBuffer(0x00);
      Serial.print("read value");
    }

    Serial.print("MB code: ");
    Serial.println(mb_error_str(res));

    vTaskDelay(1000);
  }
}

void pre(){
  digitalWrite(RE, 1);
  digitalWrite(DE, 1);
}

void post(){
  digitalWrite(RE, 0);
  digitalWrite(DE, 0);
}

void esp_get(){
  StaticJsonDocument<250> json;
  json["frequency"] = frequency;
  json["current"] = current;
  char jsonstr[250];
  serializeJson(json, jsonstr);
  
  server.send(200, "application/json", jsonstr);
}

void not_found(){
  server.send(404, "application/json", "{\"error\": \"not found\"}");
}

char *mb_error_str(uint16_t code){
  switch(code){
    case 0x01:
      return "MBIllegalFunction";
      break;
    case 0x02:
      return "MBIllegalDataAddres";
      break;
    case 0x03:
      return "MBIllegalDataValue";
      break;
    case 0x04:
      return "MBSlaveDeviceFailur";
      break;
    case 0x00:
      return "MBSuccess";
      break;
    case 0xE0:
      return "MBInvalidSlaveID";
      break;
    case 0xE1:
      return "MBInvalidFunction";
      break;
    case 0xE2:
      return "MBResponseTimedOut";
      break;
    case 0xE3:
      return "MBInvalidCRC";
      break;
  }
}
