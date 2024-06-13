/**************************************************************************
 * mobile_sensors.ino
 * by Tal Eisenberg (@eisental)
 * Written for Arduino MKR1000 with a button, 128x32 OLED display, TSL2561,
 * VEML6070, and MCP9600
 *
 * Dependencies:
 * - WiFi101
 * - ArduinoMqttClient
 * - Adafruit SSD1306
 * - Adafruit TSL2561
 * - Adafruit VEML6070 Library
 * - Adafruit GFX Library
 * - SparkFun MCP9600 Thermocouple Library
 **************************************************************************/

#include "chart.h"
#include "secrets.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_TSL2561_U.h>
#include <Adafruit_VEML6070.h>
#include <ArduinoMqttClient.h>
#include <SPI.h>
#include <SparkFun_MCP9600.h>
#include <WiFi101.h>
#include <Wire.h>

// Constants

const unsigned long sleepTime = 30 * 1000;     // ms
const unsigned long resetPressDuration = 3000; // ms

// Sensor intervals

// in frames (see delay call for frame rate)
const unsigned long displayUpdateInterval = 10;

// in milliseconds
const unsigned long lightUpdateInterval = 1000;
const unsigned long lightPhase = 1000;

const unsigned long uvUpdateInterval = 5000;
const unsigned long uvPhase = 100;

const unsigned long thermoCoupleUpdateInterval = 500;
const unsigned long thermoCouplePhase = 2000;

// MQTT configuration
/* Add a secrets.h file next to this file with the following contents:
 *
 * const char wifiSSID[] = "<wifi network name>";
 * const char wifiPass[] = "<wifi network password>";
 *
 * const char mqttHost[] = "<host>";
 * const int mqttPort = <port>;
 */

const long mqttPublishInterval = 3000; // ms
const char mqttTopic[] = "mobile_sensors/data";

// -----

// 128x32 OLED display

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_MOSI 9
#define OLED_CLK 10
#define OLED_DC 7
#define OLED_CS 8
#define OLED_RESET 13

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK,
                         OLED_DC, OLED_RESET, OLED_CS);

unsigned long lastDisplayUpdate = 0;

void setupDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  display.setRotation(2); // 180deg rotation of the display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("HI");
  display.display();
}

void displayDoneSetup() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 16);
  display.println("SETUP COMPLETE!");
  display.display();
}

// Button

#define BUTTON_PIN 14

bool buttonState = false;
unsigned long buttonPressedTime = 0;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
}

void readButton() {
  bool newButtonState = digitalRead(BUTTON_PIN) == LOW;

  if (newButtonState == buttonState) {
    return;
  }

  digitalWrite(LED_BUILTIN, newButtonState ? HIGH : LOW);
  buttonState = newButtonState;
  if (buttonState) {
    buttonPressedTime = millis();
  }
  onButtonPressed(buttonState);
}

// Error handling

void freezeOnError(char err[]) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.println("ERROR");
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.print(err);
  display.display();
  while (true) {
    readButton();
    if (buttonState) {
      display.clearDisplay();
      break;
    }
    delay(1000);
  }
}

// UV sensor (VEML6070)

Adafruit_VEML6070 uv = Adafruit_VEML6070();
Chart uvChart(&display, "UV", display.width(), display.height());

unsigned long lastUvUpdate = 0;
uint16_t uvValue = 0;

void setupUV() {
  uv.begin(VEML6070_1_T); // pass in the integration time constant}
}

void readUV() { uvValue = uv.readUV(); }

// Light sensor (TSL2561)

Adafruit_TSL2561_Unified tsl =
    Adafruit_TSL2561_Unified(TSL2561_ADDR_LOW, 12345);
Chart lightChart(&display, "LIGHT", display.width(), display.height());

unsigned long lastLightUpdate = 0;
uint16_t lightValue = 0;

void setupLight() {
  tsl.setGain(TSL2561_GAIN_1X);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);
}

void readLight() {
  sensors_event_t event;
  tsl.getEvent(&event);
  lightValue = event.light;
}

// Temp Sensor (MCP9600)

MCP9600 tempSensor;
Chart tempChart(&display, "PROBE", display.width(), display.height());
Chart ambientChart(&display, "AMBIENT", display.width(), display.height());

unsigned long lastThermoCoupleUpdate = 0;
float thermoCoupleValue = -1.0;
float ambientTempValue = -1.0;

void setupTempSensor() {
  tempSensor.begin(); // Uses the default address 0x60 for SparkFun

  // check if the sensor is connected
  if (!tempSensor.isConnected()) {
    freezeOnError("Temp Sensor not found");
  }

  // check if the Device ID is correct
  if (!tempSensor.checkDeviceID()) {
    freezeOnError("Wrong temp sensor device ID");
  }
}

void readTempSensor() {
  if (tempSensor.available()) {
    float ambient = tempSensor.getAmbientTemp();
    float delta = tempSensor.getTempDelta();

    thermoCoupleValue = ambient + delta;
    ambientTempValue = ambient;
  }
}

// MQTT

WiFiClient wifi;
MqttClient mqtt(wifi);

unsigned long lastMQTTPublish = 0;
bool publishToggle = false;

void setupMQTT() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("WiFi: ");
  display.println(wifiSSID);
  display.display();

  // TODO: try to connect in the background
  while (WiFi.begin(wifiSSID, wifiPass) != WL_CONNECTED) {
    display.print(".");
    display.display();
    readButton();
    if (buttonState) {
      break;
      buttonState = false;
    }
    delay(1000);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("Connected!");
    display.display();
  } else {
    display.println("No WiFi!");
    display.display();
    return;
  }

  delay(500);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to MQTT:");
  display.print(mqttHost);
  display.print(":");
  display.println(mqttPort);
  display.display();

  if (!mqtt.connect(mqttHost, mqttPort)) {
    char err[80];
    sprintf(err, "MQTT connect failed. Error code: %ld", mqtt.connectError());
    freezeOnError(err);
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connected to MQTT!\n");
    delay(500);
  }
}

void publishMQTT() {
  if (!mqtt.connected()) {
    return;
  }
  publishToggle = !publishToggle;
  mqtt.beginMessage(mqttTopic);
  mqtt.print("{\"probe_temp\": ");
  mqtt.print(thermoCoupleValue);
  mqtt.print(", \"ambient_temp\": ");
  mqtt.print(ambientTempValue);
  mqtt.print(", \"uv\": ");
  mqtt.print(uvValue);
  mqtt.print(", \"light\": ");
  mqtt.print(lightValue);
  mqtt.print(", \"timestamp\": ");
  mqtt.print(millis());
  mqtt.println("}");
  mqtt.endMessage();
}

// ------- main loop -------

#define ALLSENSOR_SCREEN 0
#define AMBIENT_TEMP_SCREEN 1
#define THERMOCOUPLE_SCREEN 2
#define LIGHT_SCREEN 3
#define UV_SCREEN 4
#define SCREEN_COUNT 5

unsigned long count = 0;
unsigned long wakeupTime = 0;
unsigned long lastScreenEnter = 0;
bool isAwake = true;
int curScreen = 0;
Chart *curChart = NULL;

unsigned long startTime;
bool firstLoop = true;

bool timeToUpdate(unsigned long now, unsigned long *lastUpdate,
                  unsigned long updateInterval, unsigned long phase) {
  if (now - startTime < phase) {
    return false;
  }

  bool res = now - *lastUpdate > updateInterval;

  if (res) {
    *lastUpdate = now;
  }

  return res;
}

bool countToUpdate(unsigned long updateInterval, unsigned long phase) {
  if (count < phase) {
    return false;
  }
  return (count - phase) % updateInterval == 0;
}

void drawFlag(int idx, bool val) {
  if (val) {
    display.fillRect(123, 27 - idx * 7, 5, 5, SSD1306_WHITE);
  } else {
    display.fillRect(123, 27 - idx * 7, 5, 5, SSD1306_BLACK);
  }
}

void drawAllSensorScreen() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("UV "));

  if (uvValue != 65535) {
    display.println(uvValue);
  } else {
    display.println("--");
  }

  display.setCursor(64, 0);
  display.print(F("L  "));
  if (lightValue != 65535 && lightValue != 0) {
    display.println(lightValue);
  } else {
    display.println("--");
  }

  display.setCursor(0, 20);
  display.print("T  ");

  if (thermoCoupleValue != -1) {
    display.println(thermoCoupleValue);
  } else {
    display.println("--");
  }

  display.setCursor(64, 20);
  display.print("Ta ");
  if (ambientTempValue != -1) {
    display.println(ambientTempValue);
  } else {
    display.println("--");
  }

  // flags
  drawFlag(0, buttonState);
  drawFlag(1, publishToggle);
}

void setup() {
  Serial.begin(9600);

  setupButton();
  setupDisplay();

  delay(250);

  setupMQTT();
  setupUV();
  setupLight();
  Wire.begin();
  Wire.setClock(100000);
  setupTempSensor();

  displayDoneSetup();
  delay(1000);
}

void loop() {
  mqtt.poll();

  unsigned long now = millis();
  if (firstLoop) {
    startTime = now;
    firstLoop = false;
  }

  // We only sleep in the main screen.
  if (curScreen == ALLSENSOR_SCREEN && isAwake &&
      (now - wakeupTime > sleepTime)) {
    sleep();
  }

  readButton();

  if (timeToUpdate(now, &lastUvUpdate, uvUpdateInterval, uvPhase)) {
    readUV();
    uvChart.updateChart(uvValue);
  }

  if (timeToUpdate(now, &lastLightUpdate, lightUpdateInterval, lightPhase)) {
    readLight();
    lightChart.updateChart(lightValue);
  }

  if (timeToUpdate(now, &lastThermoCoupleUpdate, thermoCoupleUpdateInterval,
                   thermoCouplePhase)) {
    readTempSensor();
    tempChart.updateChart(thermoCoupleValue);
    ambientChart.updateChart(ambientTempValue);
  }

  if (timeToUpdate(now, &lastMQTTPublish, mqttPublishInterval,
                   mqttPublishInterval)) {
    publishMQTT();
  }

  if (isAwake && countToUpdate(displayUpdateInterval, 0)) {
    display.clearDisplay();
    if (curChart == NULL) {
      drawAllSensorScreen();
    } else {
      curChart->draw();
    }

    display.display();
  }
  count += 1;

  delay(10);
}

void sleep() {
  isAwake = false;
  display.clearDisplay();
  display.display();
}

void onButtonPressed(bool pressed) {
  if (firstLoop) {
    // don't run before setup is done
    return;
  }
  unsigned long now = millis();

  if (!isAwake) {
    if (pressed) {
      // wake up
      isAwake = true;
      lastScreenEnter = now;
    }
  } else if (now - lastScreenEnter > 500) {
    if (!pressed && curChart != NULL && !curChart->getInfoMode()) {
      curChart->setInfoMode(true);
    } else {
      if (!pressed && now - buttonPressedTime > resetPressDuration) {
        curChart->reset();
      } else {
        // switch screen
        if (!pressed) {
          curScreen = (curScreen + 1) % SCREEN_COUNT;
          lastScreenEnter = now;
          switch (curScreen) {
          case ALLSENSOR_SCREEN:
            curChart = NULL;
            break;
          case UV_SCREEN:
            curChart = &uvChart;
            break;
          case LIGHT_SCREEN:
            curChart = &lightChart;
            break;
          case AMBIENT_TEMP_SCREEN:
            curChart = &ambientChart;
            break;
          case THERMOCOUPLE_SCREEN:
            curChart = &tempChart;
            break;
          }

          if (curChart != NULL) {
            curChart->start();
          }
        }
      }
    }
  }

  wakeupTime = now;
}