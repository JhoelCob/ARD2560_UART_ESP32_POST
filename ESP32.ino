// Programa para ESP32 - Sincronización NTP y envío HTTP
// Envía hora inicial al Arduino y recibe datos para enviar a API

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Configuración WiFi
const char* ssid = "TU_WIFI_SSID";           // Cambia por tu SSID
const char* password = "TU_WIFI_PASSWORD";    // Cambia por tu contraseña

// Configuración API
const char* apiURL = "https://tu-api.com/endpoint";  // Cambia por tu URL de API
const char* apiKey = "tu-api-key";                   // Opcional: tu API key

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  // GMT-5 (Perú) en segundos
const int daylightOffset_sec = 0;   // Sin horario de verano

// Pines UART para comunicación con Arduino
#define RXD2 16
#define TXD2 17
HardwareSerial arduinoSerial(2);

void setup() {
  Serial.begin(115200);
  arduinoSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  Serial.println("ESP32 iniciado");
  
  // Conectar a WiFi
  connectToWiFi();
  
  // Configurar NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Esperar sincronización NTP
  Serial.println("Esperando sincronización NTP...");
  while (!getLocalTime()) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nNTP sincronizado!");
  
  // Enviar hora inicial al Arduino
  sendInitialTimeToArduino();
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, reintentando...");
    connectToWiFi();
  }
  
  // Escuchar datos del Arduino
  receiveDataFromArduino();
  
  delay(100);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado!");
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    Serial.println();
    Serial.println("Error al conectar WiFi");
  }
}

bool getLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }
  
  Serial.println(&timeinfo, "Hora local: %A, %B %d %Y %H:%M:%S");
  return true;
}

void sendInitialTimeToArduino() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Convertir a timestamp Unix
    time_t timestamp = mktime(&timeinfo);
    
    String timeMessage = "TIME:" + String(timestamp);
    arduinoSerial.println(timeMessage);
    
    Serial.println("Hora enviada al Arduino: " + String(timestamp));
    Serial.println(&timeinfo, "Formato legible: %Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("Error al obtener la hora");
  }
}

void receiveDataFromArduino() {
  if (arduinoSerial.available()) {
    String message = arduinoSerial.readStringUntil('\n');
    message.trim();
    
    Serial.println("Mensaje del Arduino: " + message);
    
    if (message.startsWith("DATA:")) {
      String jsonData = message.substring(5);
      
      // Procesar y enviar datos a la API
      sendDataToAPI(jsonData);
    }
  }
}

void sendDataToAPI(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiURL);
    
    // Headers
    http.addHeader("Content-Type", "application/json");
    if (strlen(apiKey) > 0) {
      http.addHeader("Authorization", "Bearer " + String(apiKey));
    }
    
    // Agregar metadata del ESP32
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, jsonData);
    
    // Agregar información adicional
    doc["esp32_timestamp"] = getCurrentTimestamp();
    doc["esp32_ip"] = WiFi.localIP().toString();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();
    
    String enhancedJson;
    serializeJson(doc, enhancedJson);
    
    Serial.println("Enviando a API:");
    Serial.println(enhancedJson);
    
    // Realizar petición POST
    int httpResponseCode = http.POST(enhancedJson);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Respuesta HTTP: " + String(httpResponseCode));
      Serial.println("Respuesta: " + response);
      
      // Enviar confirmación al Arduino
      if (httpResponseCode == 200 || httpResponseCode == 201) {
        arduinoSerial.println("ACK:SUCCESS");
        Serial.println("Datos enviados exitosamente a la API");
      } else {
        arduinoSerial.println("ACK:ERROR:" + String(httpResponseCode));
        Serial.println("Error en envío a API: " + String(httpResponseCode));
      }
    } else {
      Serial.println("Error en petición HTTP: " + String(httpResponseCode));
      arduinoSerial.println("ACK:HTTP_ERROR");
    }
    
    http.end();
  } else {
    Serial.println("WiFi no conectado - no se puede enviar a API");
    arduinoSerial.println("ACK:WIFI_ERROR");
  }
}

unsigned long getCurrentTimestamp() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    return mktime(&timeinfo);
  }
  return 0;
}

// Función para debug - obtener hora actual formateada
String getCurrentTimeString() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStringBuff);
  }
  return "Error obteniendo hora";
}