/*
 * ESP32 - Sistema de Tiempo con Servidor
 * Gestión WiFi con NVS, escáner de redes y comunicación con Arduino Mega 2560
 * Se conecta a WiFi, obtiene tiempo NTP, envía tiempo inicial al Arduino
 * Recibe datos cada 2 minutos y los envía al servidor via POST
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Preferences.h>

// Objeto para manejar las preferencias (NVS)
Preferences preferences;

// Variables para almacenar credenciales WiFi
String ssid = "";
String password = "";

// Configuración WiFi
const unsigned long WIFI_TIMEOUT = 15000; // 15 segundos timeout
const String NVS_NAMESPACE = "wifi_config";

// Configuración del servidor
const char* serverURL = "http://URL_DEL_SERVIDOR"; // Cambiar por tu URL del servidor

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;  // GMT-5 (Perú) = -5 * 3600
const int daylightOffset_sec = 0;

// Variables de control del sistema principal
bool initialTimeSent = false;
bool wifiConfigured = false;
unsigned long lastDataReceived = 0;
String lastReceivedData = "";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 Sistema de Tiempo con WiFi Manager ===");
  Serial.println("Inicializando sistema...\n");
  
  // Inicializar NVS
  if (!initializeNVS()) {
    Serial.println("Error: No se pudo inicializar NVS");
    return;
  }
  
  // Gestionar conexión WiFi
  setupWiFiConnection();
  
  if (wifiConfigured) {
    // Configurar tiempo NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Esperar a que se sincronice el tiempo
    Serial.println("Sincronizando tiempo NTP...");
    delay(3000);
    
    // Enviar tiempo inicial al Arduino
    sendInitialTimeToArduino();
    initialTimeSent = true;
    
    Serial.println("Sistema ESP32 listo. Esperando datos del Arduino...");
  } else {
    Serial.println("Error: No se pudo configurar WiFi. Sistema no operativo.");
  }
}

void loop() {
  // Solo procesar si WiFi está configurado
  if (!wifiConfigured) {
    delay(5000);
    return;
  }
  
  // Verificar conexión WiFi cada 30 segundos
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) {
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("¡Conexión WiFi perdida! Reintentando...");
      if (!connectToWiFi()) {
        Serial.println("No se pudo reconectar. Sistema requiere reconfiguración.");
        wifiConfigured = false;
        return;
      }
    }
  }
  
  // Verificar si hay datos del Arduino
  if (Serial.available()) {
    String receivedData = Serial.readStringUntil('\n');
    receivedData.trim();
    
    if (receivedData.length() > 0 && isValidDateTime(receivedData)) {
      Serial.println("Datos recibidos del Arduino: " + receivedData);
      lastReceivedData = receivedData;
      lastDataReceived = millis();
      
      // Enviar datos al servidor
      sendDataToServer(receivedData);
    }
  }
  
  delay(100);
}

void setupWiFiConnection() {
  // Intentar conectar con credenciales almacenadas
  if (loadWiFiCredentials()) {
    Serial.println("Credenciales encontradas en memoria NVS");
    Serial.println("SSID: " + ssid);
    Serial.println("Intentando conectar...\n");
    
    if (connectToWiFi()) {
      Serial.println("¡Conexión WiFi exitosa!");
      printWiFiStatus();
      wifiConfigured = true;
    } else {
      Serial.println("No se pudo conectar con las credenciales almacenadas");
      requestNewCredentials();
    }
  } else {
    Serial.println("No se encontraron credenciales almacenadas");
    requestNewCredentials();
  }
}

bool initializeNVS() {
  preferences.begin(NVS_NAMESPACE.c_str(), false);
  Serial.println("Sistema NVS inicializado correctamente");
  return true;
}

bool loadWiFiCredentials() {
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  
  if (ssid.length() > 0 && password.length() > 0) {
    return true;
  }
  
  return false;
}

bool saveWiFiCredentials() {
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  
  Serial.println("Credenciales guardadas en memoria NVS");
  return true;
}

bool connectToWiFi() {
  if (ssid.length() == 0) {
    return false;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  Serial.print("Conectando a " + ssid);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    WiFi.disconnect();
    return false;
  }
}

void scanAndDisplayNetworks() {
  Serial.println("\n=== Escaneando Redes WiFi Disponibles ===");
  Serial.println("Iniciando escaneo...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int networkCount = WiFi.scanNetworks();
  
  if (networkCount == 0) {
    Serial.println("No se encontraron redes WiFi");
  } else {
    Serial.printf("\n%d redes encontradas:\n", networkCount);
    Serial.println("Nr | SSID                             | RSSI | CH | Encriptación");
    Serial.println("---|----------------------------------|------|----|--------------");
    
    for (int i = 0; i < networkCount; i++) {
      Serial.printf("%2d", i + 1);
      Serial.print(" | ");
      Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
      Serial.print(" | ");
      Serial.printf("%4ld", WiFi.RSSI(i));
      Serial.print(" | ");
      Serial.printf("%2ld", WiFi.channel(i));
      Serial.print(" | ");
      
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:            Serial.print("Abierta"); break;
        case WIFI_AUTH_WEP:             Serial.print("WEP"); break;
        case WIFI_AUTH_WPA_PSK:         Serial.print("WPA"); break;
        case WIFI_AUTH_WPA2_PSK:        Serial.print("WPA2"); break;
        case WIFI_AUTH_WPA_WPA2_PSK:    Serial.print("WPA+WPA2"); break;
        case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-EAP"); break;
        case WIFI_AUTH_WPA3_PSK:        Serial.print("WPA3"); break;
        case WIFI_AUTH_WPA2_WPA3_PSK:   Serial.print("WPA2+WPA3"); break;
        case WIFI_AUTH_WAPI_PSK:        Serial.print("WAPI"); break;
        default:                        Serial.print("Desconocida");
      }
      Serial.println();
      delay(10);
    }
  }
  
  Serial.println("=========================================\n");
  WiFi.scanDelete(); // Liberar memoria
}

void requestNewCredentials() {
  Serial.println("\n=== Configuración de Red WiFi ===");
  
  // Mostrar redes disponibles
  scanAndDisplayNetworks();
  
  Serial.println("Seleccione una red de la lista o ingrese manualmente:");
  Serial.println("Ingrese las credenciales de la red WiFi:");
  
  // Solicitar SSID
  Serial.print("SSID: ");
  while (Serial.available() == 0) {
    delay(100);
  }
  ssid = Serial.readStringUntil('\n');
  ssid.trim();
  
  // Solicitar contraseña
  Serial.print("Contraseña: ");
  while (Serial.available() == 0) {
    delay(100);
  }
  password = Serial.readStringUntil('\n');
  password.trim();
  
  Serial.println("\nCredenciales recibidas:");
  Serial.println("SSID: " + ssid);
  Serial.println("Contraseña: " + String(password.length()) + " caracteres\n");
  
  // Intentar conectar con las nuevas credenciales
  Serial.println("Intentando conectar con las nuevas credenciales...");
  if (connectToWiFi()) {
    Serial.println("¡Conexión exitosa!");
    saveWiFiCredentials();
    printWiFiStatus();
    wifiConfigured = true;
  } else {
    Serial.println("Error: No se pudo conectar con las credenciales proporcionadas");
    Serial.println("Verifique el SSID y contraseña e intente nuevamente\n");
    
    // Limpiar credenciales inválidas
    ssid = "";
    password = "";
    
    // Preguntar si desea reintentar
    Serial.println("¿Desea intentar nuevamente? (s/n)");
    while (Serial.available() == 0) {
      delay(100);
    }
    String retry = Serial.readStringUntil('\n');
    retry.trim();
    retry.toLowerCase();
    
    if (retry == "s" || retry == "si" || retry == "yes") {
      delay(1000);
      requestNewCredentials();
    } else {
      Serial.println("Configuración cancelada. Sistema no operativo.");
      wifiConfigured = false;
    }
  }
}

void printWiFiStatus() {
  Serial.println("\n=== Estado de Conexión WiFi ===");
  Serial.println("Estado: Conectado");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
  Serial.println("MAC: " + WiFi.macAddress());
  Serial.println("===============================\n");
}

void sendInitialTimeToArduino() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error obteniendo tiempo NTP");
    return;
  }
  
  // Formatear tiempo: HH:MM:SS-DD/MM/YYYY
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%H:%M:%S-%d/%m/%Y", &timeinfo);
  
  Serial.println(String(timeString));
  Serial.println("Tiempo inicial enviado al Arduino: " + String(timeString));
}

void sendDataToServer(String dateTime) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    
    // Crear JSON
    DynamicJsonDocument doc(1024);
    doc["timestamp"] = dateTime;
    doc["device"] = "ESP32-Arduino_System";
    doc["status"] = "active";
    doc["wifi_ssid"] = WiFi.SSID();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip_address"] = WiFi.localIP().toString();
    
    // Agregar timestamp Unix
    struct tm tm;
    parseDateTime(dateTime, &tm);
    time_t timestamp = mktime(&tm);
    doc["unix_timestamp"] = timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    Serial.println("Enviando al servidor: " + jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Respuesta del servidor: " + String(httpResponseCode));
      Serial.println("Contenido: " + response);
    } else {
      Serial.println("Error en POST: " + String(httpResponseCode));
    }
    
    http.end();
  } else {
    Serial.println("WiFi no conectado - No se puede enviar al servidor");
  }
}

bool isValidDateTime(String dateTime) {
  // Verificar formato básico: HH:MM:SS-DD/MM/YYYY
  if (dateTime.length() != 19) return false;
  if (dateTime.charAt(2) != ':') return false;
  if (dateTime.charAt(5) != ':') return false;
  if (dateTime.charAt(8) != '-') return false;
  if (dateTime.charAt(11) != '/') return false;
  if (dateTime.charAt(14) != '/') return false;
  
  return true;
}

void parseDateTime(String dateTime, struct tm* tm) {
  // Parsear HH:MM:SS-DD/MM/YYYY
  tm->tm_hour = dateTime.substring(0, 2).toInt();
  tm->tm_min = dateTime.substring(3, 5).toInt();
  tm->tm_sec = dateTime.substring(6, 8).toInt();
  tm->tm_mday = dateTime.substring(9, 11).toInt();
  tm->tm_mon = dateTime.substring(12, 14).toInt() - 1; // tm_mon es 0-11
  tm->tm_year = dateTime.substring(15, 19).toInt() - 1900; // tm_year desde 1900
  tm->tm_wday = 0;
  tm->tm_yday = 0;
  tm->tm_isdst = -1;
}

// Funciones adicionales de utilidad
void clearStoredCredentials() {
  preferences.remove("ssid");
  preferences.remove("password");
  Serial.println("Credenciales eliminadas de la memoria NVS");
}

void showNVSInfo() {
  Serial.println("\n=== Información de Memoria NVS ===");
  Serial.println("Namespace: " + NVS_NAMESPACE);
  
  String stored_ssid = preferences.getString("ssid", "No encontrado");
  String stored_pass = preferences.getString("password", "No encontrado");
  
  Serial.println("SSID almacenado: " + stored_ssid);
  Serial.println("Contraseña almacenada: " + (stored_pass != "No encontrado" ? String(stored_pass.length()) + " caracteres" : "No encontrada"));
  Serial.println("================================\n");
}
