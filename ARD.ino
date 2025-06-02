// Arduino - ESP32 - Recepcion y envio de datos por tiempo via UART
// Recibe hora inicial del ESP32 y envía datos cada 2 minutos

#include <SoftwareSerial.h>

// Configuración de pines para comunicación UART
SoftwareSerial espSerial(2, 3); // RX, TX

// Variables para manejo de tiempo
unsigned long currentTime = 0;          // Tiempo actual en segundos desde epoch
unsigned long lastSyncTime = 0;         // Última vez que se sincronizó
unsigned long lastSendTime = 0;         // Última vez que se enviaron datos
bool timeReceived = false;              // Flag para verificar si se recibió la hora

const unsigned long SEND_INTERVAL = 120000; // 2 minutos (Modificar despues)
const unsigned long SECOND_INTERVAL = 1000; // 1 segundo

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  
  Serial.println("Arduino iniciado - Esperando la obtencion de datos ESP32");
  
  // Esperar a recibir la hora inicial del ESP32
  while (!timeReceived) {
    receiveTimeFromESP32();
    delay(100);
  }
  
  Serial.println("Hora recibida correctamente!");
  lastSendTime = millis();
}

void loop() {
  // Actualizar tiempo cada segundo
  static unsigned long lastSecondUpdate = 0;
  if (millis() - lastSecondUpdate >= SECOND_INTERVAL) {
    currentTime++;
    lastSecondUpdate = millis();
  }
  
  // Enviar datos cada 2 minutos
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendTimeToESP32();
    lastSendTime = millis();
  }
  
  // Verificar si hay comandos desde el monitor serie
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "TIME") {
      printCurrentTime();
    } else if (command == "SEND") {
      sendTimeToESP32();
    }
  }
}

void receiveTimeFromESP32() {
  if (espSerial.available()) {
    String message = espSerial.readStringUntil('\n');
    message.trim();
    
    Serial.println("Mensaje recibido: " + message);
    
    if (message.startsWith("TIME:")) {
      // Extraer el timestamp
      String timeStr = message.substring(5);
      currentTime = timeStr.toInt();
      
      if (currentTime > 0) {
        timeReceived = true;
        lastSyncTime = millis();
        Serial.println("Tiempo sincronizado: " + String(currentTime));
        printCurrentTime();
      }
    }
  }
}

void sendTimeToESP32() {
  // Crear mensaje JSON con datos de tiempo
  String jsonMessage = "{";
  jsonMessage += "\"timestamp\":" + String(currentTime) + ",";
  jsonMessage += "\"device\":\"Arduino\",";
  jsonMessage += "\"uptime\":" + String(millis()) + ",";
  jsonMessage += "\"formatted_time\":\"" + formatTime(currentTime) + "\"";
  jsonMessage += "}";
  
  // Enviar al ESP32
  espSerial.println("DATA:" + jsonMessage);
  
  Serial.println("Datos enviados al ESP32:");
  Serial.println(jsonMessage);
}

String formatTime(unsigned long timestamp) {
  // Convertir timestamp a formato legible (básico)
  unsigned long seconds = timestamp % 60;
  unsigned long minutes = (timestamp / 60) % 60;
  unsigned long hours = (timestamp / 3600) % 24;
  unsigned long days = timestamp / 86400;
  
  String formatted = String(days) + "d ";
  if (hours < 10) formatted += "0";
  formatted += String(hours) + ":";
  if (minutes < 10) formatted += "0";
  formatted += String(minutes) + ":";
  if (seconds < 10) formatted += "0";
  formatted += String(seconds);
  
  return formatted;
}

void printCurrentTime() {
  Serial.println("Tiempo actual: " + String(currentTime) + " (" + formatTime(currentTime) + ")");
}
