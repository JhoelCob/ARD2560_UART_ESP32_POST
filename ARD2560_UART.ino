/*
 * Arduino Mega 2560 Rev3 - Sistema de Tiempo con ESP32
 * Recibe datos de tiempo del ESP32 via UART
 * Envía datos cada 2 minutos de vuelta al ESP32
 */

String receivedDateTime = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 120000; // 2 minutos en milisegundos
bool timeReceived = false;
bool systemInitialized = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Arduino Mega 2560 - Sistema de Tiempo Iniciado");
  Serial.println("Esperando datos de tiempo del ESP32...");
  
  // Inicializar variables
  receivedDateTime = "";
  lastSendTime = 0;
  timeReceived = false;
  systemInitialized = false;
}

void loop() {
  // Verificar si hay datos disponibles en UART
  if (Serial.available()) {
    String incomingData = Serial.readStringUntil('\n');
    incomingData.trim();
    
    if (incomingData.length() > 0) {
      Serial.println("Datos recibidos del ESP32: " + incomingData);
      
      // Validar formato de fecha/hora (HH:MM:SS-DD/MM/YYYY)
      if (isValidDateTime(incomingData)) {
        receivedDateTime = incomingData;
        timeReceived = true;
        
        if (!systemInitialized) {
          // Primera vez que recibe datos - inicializar sistema
          systemInitialized = true;
          lastSendTime = millis();
          Serial.println("Sistema inicializado con tiempo: " + receivedDateTime);
        } else {
          // Reiniciar el programa cuando llegan nuevos datos
          Serial.println("Nuevos datos recibidos - Reiniciando sistema...");
          delay(1000);
          resetSystem();
        }
      } else {
        Serial.println("Formato de fecha/hora inválido: " + incomingData);
      }
    }
  }
  
  // Si el sistema está inicializado y han pasado 2 minutos, enviar datos
  if (systemInitialized && timeReceived && (millis() - lastSendTime >= SEND_INTERVAL)) {
    sendTimeToESP32();
    lastSendTime = millis();
  }
  
  delay(100); // Pequeña pausa para evitar saturar el procesador
}

void sendTimeToESP32() {
  if (receivedDateTime != "") {
    // Calcular nuevo tiempo (agregar 2 minutos al tiempo recibido)
    String newDateTime = addMinutesToDateTime(receivedDateTime, 2);
    
    Serial.println(newDateTime);
    Serial.println("Datos enviados al ESP32: " + newDateTime);
    
    // Actualizar el tiempo interno
    receivedDateTime = newDateTime;
  }
}

String addMinutesToDateTime(String dateTime, int minutesToAdd) {
  // Parsear la fecha/hora actual
  int hourStart = dateTime.indexOf(':') - 2;
  int minuteStart = dateTime.indexOf(':') + 1;
  int secondStart = dateTime.indexOf(':') + 4;
  
  String hourStr = dateTime.substring(hourStart, hourStart + 2);
  String minuteStr = dateTime.substring(minuteStart, minuteStart + 2);
  String secondStr = dateTime.substring(secondStart, secondStart + 2);
  
  int hour = hourStr.toInt();
  int minute = minuteStr.toInt();
  int second = secondStr.toInt();
  
  // Agregar minutos
  minute += minutesToAdd;
  
  // Manejar overflow de minutos
  if (minute >= 60) {
    hour += minute / 60;
    minute = minute % 60;
  }
  
  // Manejar overflow de horas
  if (hour >= 24) {
    hour = hour % 24;
    // Aquí podrías agregar lógica para cambiar la fecha si es necesario
  }
  
  // Reconstruir la cadena de fecha/hora
  String newHour = (hour < 10) ? "0" + String(hour) : String(hour);
  String newMinute = (minute < 10) ? "0" + String(minute) : String(minute);
  String newSecond = (second < 10) ? "0" + String(second) : String(second);
  
  String datePart = dateTime.substring(dateTime.indexOf('-'));
  
  return newHour + ":" + newMinute + ":" + newSecond + datePart;
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

void resetSystem() {
  // Simular reinicio del sistema
  setup();
}