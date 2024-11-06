#include <Adafruit_NeoPixel.h>  // Librería para NeoPixel
#include "DHT.h"                // Librería DHT
#include "SinricPro.h"
#include "SinricProTemperaturesensor.h"
#include "SinricProLight.h"     // Librería para dispositivos tipo Light

#define WIFI_SSID         "your_ssid"
#define WIFI_PASS         "your_pass"
#define APP_KEY           "918803be-52e7-406b-9ce5-e66c9d055acc"
#define APP_SECRET        "36fa3a7f-22d4-4b4d-86a8-7913b825f467-dbcdb86a-124e-4a9a-ad75-a18121413b2e"
#define TEMP_SENSOR_ID    "672394a0a5ea880b6cb64cef" 
#define NEOPIXEL_ID       "672406dba5ea880b6cb69997"  // ID del NeoPixel en SinricPro
#define DHT_PIN           D6
#define NEOPIXEL_PIN      D1  // Pin donde está conectado el anillo NeoPixel
#define BAUD_RATE         115200              
#define EVENT_WAIT_TIME   60000

#define DHT_TYPE          DHT11
DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(16, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800); // NeoPixel de 16 LEDs

float temperature;
float humidity;
float lastTemperature;
float lastHumidity;
unsigned long lastEvent = (-EVENT_WAIT_TIME);
bool sensorPowerState = true;  // El estado inicial del sensor está encendido
int currentBrightness = 100;   // Brillo inicial al 100%

// Función para controlar el estado de encendido/apagado del sensor DHT
bool onPowerStatesensor(const String &deviceId, bool &state) {
  return true; // request handled properly
}

// Función para controlar el estado de encendido/apagado del NeoPixel
bool onLightPowerState(const String &deviceId, bool &state) {
  Serial.printf("NeoPixel turned %s (via SinricPro)\r\n", state ? "on" : "off");
  
  if (state) {
    // Encender todos los LEDs en color blanco
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(255 * currentBrightness / 100, 255 * currentBrightness / 100, 255 * currentBrightness / 100));  // Blanco con brillo ajustado
    }
    strip.show();
  } else {
    // Apagar los LEDs
    strip.clear();
    strip.show();
  }

  return true;
}

// Función para cambiar el color del NeoPixel usando SinricPro
bool onLightSetColor(const String &deviceId, byte r, byte g, byte b) {
  Serial.printf("Setting NeoPixel color to RGB(%d, %d, %d)\r\n", r, g, b);
  
  // Cambiar el color de todos los LEDs al color recibido con el brillo actual
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r * currentBrightness / 100, g * currentBrightness / 100, b * currentBrightness / 100));
  }
  strip.show();

  return true;  // Indica que el comando fue procesado correctamente
}

// Función para cambiar el brillo del NeoPixel usando SinricPro
bool onLightSetBrightness(const String &deviceId, int &brightness) {
  Serial.printf("Setting NeoPixel brightness to %d%%\r\n", brightness);
  
  currentBrightness = brightness;  // Actualizar el brillo actual
  
  // Reaplicar el color actual de los LEDs con el nuevo brillo
  uint32_t color = strip.getPixelColor(0);  // Obtener el color del primer LED (como referencia)
  byte r = (color >> 16) & 0xFF;
  byte g = (color >> 8) & 0xFF;
  byte b = color & 0xFF;

  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r * currentBrightness / 100, g * currentBrightness / 100, b * currentBrightness / 100));
  }
  strip.show();

  return true;  // Indica que el comando fue procesado correctamente
}

void handleTemperaturesensor() {
  // Verificar si el sensor está encendido
  if (!sensorPowerState) return;

  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) return;

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT reading failed!");
    return;
  }

  if (temperature == lastTemperature && humidity == lastHumidity) return;

  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  bool success = mySensor.sendTemperatureEvent(temperature, humidity);
  if (success) {
    Serial.printf("Temperature: %2.1f Celsius\tHumidity: %2.1f%%\r\n", temperature, humidity);
  } else {
    Serial.println("Something went wrong...could not send Event to server!");
  }

  lastTemperature = temperature;
  lastHumidity = humidity;
  lastEvent = actualMillis;
}

void setupWiFi() {
  Serial.print("[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  IPAddress localIP = WiFi.localIP();
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
}

void setupSinricPro() {
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
 // mySensor.onPowerState(onPowerStatesensor);   // Asignar callback para encender/apagar el sensor DHT
  
  SinricProLight &myLight = SinricPro[NEOPIXEL_ID];  // Configurar dispositivo NeoPixel
  myLight.onPowerState(onLightPowerState);           // Asignar callback para encender/apagar el NeoPixel
  myLight.onColor(onLightSetColor);                  // Asignar callback para cambiar el color
  myLight.onBrightness(onLightSetBrightness);        // Asignar callback para cambiar el brillo

  SinricPro.onConnected([]() {
    Serial.println("Connected to SinricPro");
  });

  SinricPro.onDisconnected([]() {
    Serial.println("Disconnected from SinricPro");
  });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(BAUD_RATE); 
  Serial.println("\r\n\r\n");

  dht.begin();
  strip.begin();  // Inicializar NeoPixel
  strip.clear();  // Apagar NeoPixel inicialmente
  strip.show();

  setupWiFi();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  handleTemperaturesensor();
}