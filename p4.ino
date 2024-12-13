#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SGP30.h>
#include <WiFi.h>
#include "time.h"
#include <Freenove_WS2812B_RGBLED_Controller.h>

// Configuration WiFi
const char* ssid = "iPhone Gaetan";
const char* password = "12345678";

// Configuration fuseau horaire
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 1;
const int   daylightOffset_sec = 3600 * 0;

// Définitions des capteurs et écran
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SGP30 sgp30;
Adafruit_SSD1306 display(128, 64, &Wire);

// LED adressable WS2812B
#define LED_COUNT 1
#define LED_PIN 26
Freenove_WS2812B_Controller led(LED_COUNT, LED_PIN, TYPE_GRB);

// Joystick et capteur capacitif
#define JOYSTICK_PIN 34
#define CAPACITIVE_SENSOR_PIN 2

// Batterie
#define BATTERY_PIN 23
#define BATTERY_MIN 2.36
#define BATTERY_MAX 3.30

// Variables globales
float humidite, temperature;
static struct tm timeinfo;
int currentState = 1;  // État initial

void setup() 
{
  Serial.begin(115200);
  Wire.begin(13, 14); //sda est sur le pin 13, scl est sur le pin 14
  WiFi.begin(ssid, password);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (!sht4.begin()) 
  {
    Serial.println("Erreur SHT40");
    while (1);
  }
  if (!sgp30.begin()) 
  {
    Serial.println("Erreur SGP30");
    while (1);
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) 
  {
    Serial.println("Erreur OLED");
    while (1);
  }

  display.display();
  delay(1000);
  display.clearDisplay();
  led.begin();

  // Configuration des broches
  pinMode(JOYSTICK_PIN, INPUT); //l'axe x du joystick est configuré en entré
  pinMode(CAPACITIVE_SENSOR_PIN, INPUT); //le capteur capacitif est configuré en entré

  // Initialisation de l'ADC
  analogReadResolution(12);  // Résolution de 12 bits (valeurs de 0 à 4095)
  analogSetAttenuation(ADC_11db);  // Étendue de 0 à ~3,3 V pour l'ADC
}

void loop() 
{
  getLocalTime(&timeinfo);
  readInputs(); 
  handleStateMachine();
  RGB_CO2();
  delay(200);
}

void readInputs() 
{
  int joystickValue = analogRead(JOYSTICK_PIN);  // lecture du joystick (0-4095)
  bool capacitiveSensor = digitalRead(CAPACITIVE_SENSOR_PIN); //lecture du capteur capacitif

  // Debug des valeurs lues
  Serial.print("Joystick Value: ");
  Serial.println(joystickValue);
  Serial.print("Capacitive Sensor: ");
  Serial.println(capacitiveSensor);

  //Transitions machine à état
  // Transitions avec le joystick
  if (joystickValue > 3600 && currentState <= 5) // si le joystick est à droite et que l'état actuel est inférieur ou égal à 5
  {  
    if(currentState == 5) //si l'état actuel est le 5
    {
      currentState = 1; //l'état passe à 1
    }
    else currentState += 1; //sinon l'état est incrémenté de 1
    Serial.println("Transition vers l'état suivant via le joystick.");
  } 
  if (joystickValue < 1000 && currentState <= 5) //  si le joystick est à gauche et que l'état actuel est inférieur ou égal à 5
  {  
    if(currentState == 1) //si l'état actuel est le 1
    {
      currentState = 5; //l'état passe à 5
    }
    else currentState -= 1; //sinon l'état est décrémenté de 1
    Serial.println("Transition vers l'état précédent via le joystick.");
  }

  // Transitions avec le capteur capacitif
  if (capacitiveSensor) //si le capteur capacitif est touché
  {
    if (currentState <= 5) //si l'état est inférieur ou égal à 5
    {
      currentState += 5;  // l'état est incrémenté de 5 (passer en état bloqué)
      Serial.println("Passage en mode bloqué via le capteur capacitif.");
    } 
    else // sinon (état supérieur à 5)
    {
      currentState -= 5;  // l'état est décrémenté de 5(retour à l'état normal)
      Serial.println("Retour en mode normal via le capteur capacitif.");
    }
  }
}

//Machine à état
void handleStateMachine() 
{
  Serial.print("État actuel : ");
  Serial.println(currentState);

  // Actions selon l'état
  switch (currentState) 
  {
    case 1: displayDateTimeBattery(&timeinfo); break; //état 1 -> affiche la date, l'heure et la batterie
    case 2: displayTemperature(); break; //état 2 -> affiche la température
    case 3: displayCO2(); break; //état 3 -> affiche le niveau de CO2
    case 4: displayHumidity(); break; //état 4 -> affiche le niveau d'humidité
    case 5: displayGlobal(&timeinfo); break; //état 5 -> affiche la température, le niveau de CO2, le niveau d'humidité, la date et l'heure
    case 6: displayDateTimeBattery(&timeinfo); break; //état 6 -> affiche la date, l'heure et la batterie (état 1 bloqué)
    case 7: displayTemperature(); break; //état 7 -> affiche la température (état 2 bloqué)
    case 8: displayCO2(); break; //état 8 -> affiche le niveau de CO2 (état 3 bloqué)
    case 9: displayHumidity(); break; //état 9 -> affiche le niveau d'humidité (état 4 bloqué)
    case 10: displayGlobal(&timeinfo); break; //état 10 -> affiche la température, le niveau de CO2, le niveau d'humidité, la date et l'heure (état 5 bloqué)
  }
}

void displayDateTimeBattery(struct tm* timeinfo) 
{
  float batteryVoltage = analogRead(BATTERY_PIN) * (3.3 / 4095.0);
  int batteryLevel = map(batteryVoltage * 100, BATTERY_MIN * 100, BATTERY_MAX * 100, 0, 100);

  // Affichage sur le terminal
  Serial.printf("Date et Heure : %02d/%02d/%04d %02d:%02d:%02d\n", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  if(batteryVoltage==0)
  {
    Serial.printf("Batterie non branchée");
  }
  else
  {
    Serial.printf("Batterie : %d%% (%.2fV)\n", batteryLevel, batteryVoltage);
  }

  //Affichage sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Date : %02d/%02d/%04d\n", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
  display.setCursor(0, 16);
  display.printf("Heure : %02d:%02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  display.setCursor(0, 32);
  if(batteryVoltage==0)
  {
    display.printf("Batterie non branchee");
  }
  else
  {
    display.printf("Batterie : %d%% (%.2fV)\n", batteryLevel, batteryVoltage);
  }
  display.display();
  display.display();
}


void displayTemperature() 
{
  sensors_event_t humidity_event, temp_event;
  sht4.getEvent(&humidity_event, &temp_event);
  temperature = temp_event.temperature; //relève la température

  Serial.printf("Température : %.2f°C\n", temperature); //affiche sur le terminal

 //Affichage sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Temp: %.2f C\n", temperature);
  display.display();
}

void displayCO2() 
{
  sgp30.IAQmeasure(); //relève le niveau de C02

  Serial.printf("CO2 : %d ppm\n", sgp30.eCO2); //affiche le niveau de C02 dans le terminal

  //Affichage du niveau de CO2 sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("CO2: %d ppm\n", sgp30.eCO2);
  display.setCursor(0, 12);
  if(sgp30.eCO2 <= 600) //good -> Green
  {
    Serial.printf("Bien");
    display.printf("Bien");
  }
  else if(sgp30.eCO2 > 600 && sgp30.eCO2 <= 800) //correct -> yellow
  {
    Serial.printf("Correcte");
    display.printf("Correcte");
  }
  else if(sgp30.eCO2 > 800 && sgp30.eCO2 <= 1100) //tolerable -> orange
  {
    Serial.printf("Tolerable");
    display.printf("Tolerable");
  }
  else if(sgp30.eCO2 > 1100 && sgp30.eCO2 <= 1400) //limit -> brown
  {
    Serial.printf("Limite");
    display.printf("Limite");
  }
  else if(sgp30.eCO2 > 1400) //bad -> red
  {
    Serial.printf("Mauvais");
    display.printf("Mauvais");
  }
  display.display();
}

void displayHumidity() 
{
  sensors_event_t humidity_event, temp_event;
  sht4.getEvent(&humidity_event, &temp_event);
  humidite = humidity_event.relative_humidity; //relève le niveau d'humidité

  Serial.printf("Humidité : %.2f%%\n", humidite); //affiche le niveau d'humidité dans le terminal

  //Affichage de l'humidité sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Humidite : %.2f%%\n", humidite);
  display.display();
}

void displayGlobal(struct tm* timeinfo) 
{
  //Affichage dans le terminal
  // température
  Serial.print("Température : ");
  Serial.print(temperature);
  Serial.println(" °C");

  // humidité
  Serial.print("Humidité : ");
  Serial.print(humidite);
  Serial.println(" %");

  // CO2
  Serial.print("CO2 : ");
  Serial.print(sgp30.eCO2);
  Serial.println(" ppm");

  //Date et heure
  Serial.printf("Date et Heure : %02d/%02d/%04d %02d:%02d:%02d\n", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

  //Affichage sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // température
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  // humidité
  display.setCursor(0, 16);
  display.print("Humid: ");
  display.print(humidite);
  display.println(" %");

  // CO2
  display.setCursor(0, 32);
  display.print("CO2: ");
  display.print(sgp30.eCO2);
  display.println(" ppm");
  
  // Date et heure
  display.setCursor(0, 48);
  display.printf("%02d:%02d:%02d %02d/%02d/%04d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);

  display.display();  
}

void RGB_CO2(void) //couleur de la led en fonction du niveau de CO2
{
 if(sgp30.eCO2 <= 600) //good -> Green
  {
    led.setLedColorData(0, 0, 255, 0);
  }
  else if(sgp30.eCO2 > 600 && sgp30.eCO2 <= 800) //correct -> yellow
  {
    led.setLedColorData(0, 255, 255, 0);
  }
  else if(sgp30.eCO2 > 800 && sgp30.eCO2 <= 1100) //tolerable -> orange
  {
    led.setLedColorData(0, 255, 128, 0);
  }
  else if(sgp30.eCO2 > 1100 && sgp30.eCO2 <= 1400) //limit -> brown
  {
    led.setLedColorData(0, 96, 64, 0);
  }
  else if(sgp30.eCO2 > 1400) //bad -> red
  {
    led.setLedColorData(0, 255, 0, 0);
  }
}