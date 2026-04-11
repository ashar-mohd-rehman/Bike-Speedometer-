/*
 * GPS Bike Speedometer for XIAO ESP32-S3
 * Hardware:
 * - XIAO ESP32-S3
 * - NEO-6M GPS Module (UART)
 * - SSD1306 OLED 128x64 (I2C)
 * - 18650 Li-ion battery with voltage divider
 * 
 * Connections:
 * GPS: TX->GPIO7(RX), RX->GPIO6(TX), VCC->3.3V, GND->GND
 * OLED: SCL->GPIO6, SDA->GPIO5, VCC->3.3V, GND->GND
 * Battery: Through switch to BAT pin, voltage divider to GPIO1
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Pin definitions
#define GPS_RX 7   // GPS TX connects here
#define GPS_TX 6   // GPS RX connects here
#define VBAT_PIN 1 // Battery voltage sensing (ADC)
#define SDA_PIN 5  // I2C SDA
#define SCL_PIN 6  // I2C SCL

// GPS UART (using Serial1)
HardwareSerial GPS_Serial(1);

// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TinyGPSPlus gps;

// Variables
float currentSpeed = 0.0;
float tripDistance = 0.0;
float maxSpeed = 0.0;
unsigned long tripStartTime = 0;
unsigned long elapsedTime = 0;
int batteryPercent = 0;
int satellites = 0;
float altitude = 0.0;
bool gpsValid = false;

// Previous position for distance calculation
double prevLat = 0.0;
double prevLon = 0.0;
bool firstFix = true;

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.setTextSize(2);
  display.println(F("GPS BIKE"));
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.println(F("Initializing..."));
  display.display();
  delay(2000);
  
  // Initialize GPS
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  
  // Initialize trip timer
  tripStartTime = millis();
  
  Serial.println(F("GPS Bike Speedometer Started!"));
}

void loop() {
  // Read GPS data
  while (GPS_Serial.available() > 0) {
    char c = GPS_Serial.read();
    gps.encode(c);
  }
  
  // Update GPS data if valid
  if (gps.location.isUpdated()) {
    gpsValid = true;
    
    // Get current speed (km/h)
    currentSpeed = gps.speed.kmph();
    
    // Update max speed
    if (currentSpeed > maxSpeed) {
      maxSpeed = currentSpeed;
    }
    
    // Calculate trip distance
    if (gps.location.isValid()) {
      double lat = gps.location.lat();
      double lon = gps.location.lng();
      
      if (!firstFix && prevLat != 0.0 && prevLon != 0.0) {
        // Calculate distance between current and previous position
        double distKm = TinyGPSPlus::distanceBetween(prevLat, prevLon, lat, lon) / 1000.0;
        
        // Only add if speed > 1 km/h (filter out GPS drift when stationary)
        if (currentSpeed > 1.0 && distKm < 0.5) { // Max 500m between updates (sanity check)
          tripDistance += distKm;
        }
      }
      
      prevLat = lat;
      prevLon = lon;
      firstFix = false;
    }
    
    // Get altitude
    if (gps.altitude.isValid()) {
      altitude = gps.altitude.meters();
    }
  }
  
  // Get satellite count
  if (gps.satellites.isUpdated()) {
    satellites = gps.satellites.value();
  }
  
  // Update elapsed time
  elapsedTime = (millis() - tripStartTime) / 1000; // seconds
  
  // Read battery level
  batteryPercent = getBatteryPercent();
  
  // Update display
  updateDisplay();
  
  delay(500); // Update every 500ms
}

int getBatteryPercent() {
  // Read ADC (average of 10 samples)
  int sum = 0;
  for(int i = 0; i < 10; i++) {
    sum += analogRead(VBAT_PIN);
    delay(10);
  }
  float avg = sum / 10.0;
  
  // ESP32-S3 ADC: 0-4095 = 0-3.3V
  // Voltage divider: multiply by 2
  float voltage = (avg / 4095.0) * 3.3 * 2.0;
  
  // 18650 range: 4.2V (100%) to 3.0V (0%)
  if(voltage >= 4.2) return 100;
  if(voltage <= 3.0) return 0;
  
  int percent = ((voltage - 3.0) / (4.2 - 3.0)) * 100;
  return constrain(percent, 0, 100);
}

void updateDisplay() {
  display.clearDisplay();
  
  // --- LEFT SIDE ---
  
  // SPEED label (top left)
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SPEED"));
  
  // Current speed (BIG)
  display.setTextSize(3);
  display.setCursor(0, 12);
  if (gpsValid && currentSpeed > 0) {
    display.print(currentSpeed, 1);
  } else {
    display.print(F("--"));
  }
  
  // KM unit
  display.setTextSize(1);
  display.setCursor(90, 20);
  display.print(F("KM"));
  display.setCursor(90, 30);
  display.print(F("H"));
  
  // Distance
  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("Distance"));
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(tripDistance, 2);
  display.print(F(" KM"));
  
  // Dashed divider line (vertical)
  for(int y = 0; y < 64; y += 4) {
    display.drawPixel(84, y, SSD1306_WHITE);
    display.drawPixel(84, y+1, SSD1306_WHITE);
  }
  
  // Horizontal dashed line below distance
  for(int x = 0; x < 84; x += 4) {
    display.drawPixel(x, 47, SSD1306_WHITE);
    display.drawPixel(x+1, 47, SSD1306_WHITE);
  }
  
  // --- RIGHT SIDE ---
  
  // Battery icon and percentage (top right)
  drawBattery(105, 0, batteryPercent);
  display.setTextSize(1);
  display.setCursor(88, 2);
  display.print(batteryPercent);
  display.print(F("%"));
  
  // Date
  display.setTextSize(1);
  display.setCursor(88, 14);
  display.print(F("Date"));
  display.setCursor(88, 24);
  if (gps.date.isValid()) {
    display.print(gps.date.day());
    display.print(F("-"));
    display.print(gps.date.month());
    display.print(F("-"));
    display.print(gps.date.year() % 100); // Last 2 digits of year
  } else {
    display.print(F("--"));
  }
  
  // Altitude
  display.setCursor(88, 34);
  display.print(F("Alt"));
  display.setCursor(88, 44);
  if (gps.altitude.isValid()) {
    display.print((int)altitude);
    display.print(F("m"));
  } else {
    display.print(F("--"));
  }
  
  // GPS Satellites
  display.setCursor(88, 54);
  display.print(F("GPS:"));
  display.print(satellites);
  
  // TIME (bottom left)
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("TIME-"));
  
  // Format elapsed time as HH:MM
  int hours = elapsedTime / 3600;
  int minutes = (elapsedTime % 3600) / 60;
  int seconds = elapsedTime % 60;
  
  if (hours < 10) display.print(F("0"));
  display.print(hours);
  display.print(F(":"));
  if (minutes < 10) display.print(F("0"));
  display.print(minutes);
  
  display.display();
}

void drawBattery(int x, int y, int percent) {
  // Battery body (16x8 pixels)
  display.drawRect(x, y+2, 16, 8, SSD1306_WHITE);
  
  // Battery tip
  display.fillRect(x+16, y+4, 2, 4, SSD1306_WHITE);
  
  // Fill level (based on percentage)
  int fillWidth = map(percent, 0, 100, 0, 14);
  if (fillWidth > 0) {
    display.fillRect(x+1, y+3, fillWidth, 6, SSD1306_WHITE);
  }
  
  // Low battery warning (invert if < 20%)
  if (percent < 20) {
    display.fillRect(x, y+2, 16, 8, SSD1306_INVERSE);
  }
}
