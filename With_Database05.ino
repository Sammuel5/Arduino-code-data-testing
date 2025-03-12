#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <TimeLib.h>  // Time library instead of RTC

// OLED Display Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// QR Scanner Serial Setup
#define RX_PIN 2  
#define TX_PIN 3  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);

// LED Pins
#define RED_LED 4    
#define GREEN_LED 5  

// WiFi & Firebase Setup
#define WIFI_SSID "Shayn"
#define WIFI_PASSWORD "123456789"
#define FIREBASE_HOST "sammuel-17249-default-rtdb.firebaseio.com"

WiFiSSLClient wifiClient;
HttpClient httpClient(wifiClient, "sammuel-17249-default-rtdb.firebaseio.com", 443);

// Month names array for formatting
const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool locked = true; 
bool scanComplete = false; 

// Initial time setup - set this to your current date and time
// Format: year, month, day, hour, minute, second
// Example: March 13, 2025, 15:30:00
#define INITIAL_YEAR 2025
#define INITIAL_MONTH 3
#define INITIAL_DAY 13
#define INITIAL_HOUR 23
#define INITIAL_MINUTE 32
#define INITIAL_SECOND 0

void displayMessage(const char *message);
void displayQRData(String data);
void sendDataToFirebase(String qrData, bool isTimeIn);
String readQRData();
void resetLEDs();
void unlockPC();
void lockPC();
void connectToWiFi();
String getFormattedDate();
String getFormattedTime();
void setupTime();

void setup() {
    Serial.begin(115200);
    u8g2.begin();

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);

    displayMessage("Initializing...");
    delay(1500);

    qrScanner.begin(9600);
    Keyboard.begin();  

    // Setup internal time
    setupTime();
    Serial.println("Time system initialized");

    digitalWrite(RED_LED, HIGH);
    displayMessage("System Locked");
    delay(1500);

    connectToWiFi();
}

void setupTime() {
    // Set the time to the values defined in the initial constants
    setTime(INITIAL_HOUR, INITIAL_MINUTE, INITIAL_SECOND, INITIAL_DAY, INITIAL_MONTH, INITIAL_YEAR);
    Serial.println("Time set to: " + getFormattedDate() + " " + getFormattedTime());
}

void loop() {
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();

        if (qrData.length() == 11 && qrData.startsWith("UA") && qrData.substring(2).toInt() > 0) {  
            scanComplete = true;  
            Serial.println("QR Data: " + qrData);
            displayQRData(qrData);
            
            if (locked) {  
                // This is a time-in scan
                sendDataToFirebase(qrData, true);
                
                unlockPC();
                digitalWrite(GREEN_LED, HIGH);
                digitalWrite(RED_LED, LOW);
                locked = false;
                displayMessage("Scan To Lock");  
            } else {  
                // This is a time-out scan
                sendDataToFirebase(qrData, false);
                
                lockPC();
                digitalWrite(RED_LED, HIGH);
                digitalWrite(GREEN_LED, LOW);
                locked = true;
                displayMessage("Scan To Unlock");  
            }

            delay(2000);  
            scanComplete = false;  
        }
    }
}

String getFormattedDate() {
    // Format date as "Mmm dd yyyy" (e.g., "Mar 14 2025")
    char dateBuffer[12];
    sprintf(dateBuffer, "%s %02d %04d", 
            monthNames[month() - 1], day(), year());
    return String(dateBuffer);
}

String getFormattedTime() {
    // Format time as "hh:mm:ss" (e.g., "14:30:45")
    char timeBuffer[9];
    sprintf(timeBuffer, "%02d:%02d:%02d", 
            hour(), minute(), second());
    return String(timeBuffer);
}

void sendDataToFirebase(String qrData, bool isTimeIn) {
    // Get current date and time at the moment of scanning
    String formattedDate = getFormattedDate();
    String formattedTime = getFormattedTime();
    
    // Create a path for the QR code
    String path = "/scannedData.json";
    String jsonData;
    
    if (isTimeIn) {
        // Time-in record
        jsonData = "{\"qrCode\": \"" + qrData + "\", "
                + "\"date\": \"" + formattedDate + "\", "
                + "\"timeIn\": \"" + formattedTime + "\", "
                + "\"status\": \"in\"}";
    } else {
        // Time-out record
        jsonData = "{\"qrCode\": \"" + qrData + "\", "
                + "\"date\": \"" + formattedDate + "\", "
                + "\"timeOut\": \"" + formattedTime + "\", "
                + "\"status\": \"out\"}";
    }

    Serial.println("Sending data to Firebase...");
    Serial.println("Host: " + String(FIREBASE_HOST));
    Serial.println("Data: " + jsonData);

    httpClient.setTimeout(5000);
    httpClient.beginRequest();
    httpClient.post(path);
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonData.length());
    httpClient.beginBody();
    httpClient.print(jsonData);
    httpClient.endRequest();

    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();

    Serial.print("HTTP Status Code: ");
    Serial.println(statusCode);
    Serial.println("Firebase Response: " + response);

    if (statusCode == 200) {
        Serial.println("✅ Data sent successfully!");
        
        // Display time info on OLED
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_mr);
        u8g2.drawStr(0, 10, "QR Scanned:");
        u8g2.drawStr(0, 20, qrData.c_str());
        u8g2.drawStr(0, 30, isTimeIn ? "Time IN:" : "Time OUT:");
        u8g2.drawStr(0, 40, formattedTime.c_str());
        u8g2.drawStr(0, 50, formattedDate.c_str());
        u8g2.sendBuffer();
    } else {
        Serial.println("❌ Failed to send data!");
    }
}

void displayMessage(const char *message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, message);
    u8g2.sendBuffer();
}

void displayQRData(String data) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_mr);
    u8g2.drawStr(0, 10, "QR Scanned:");
    u8g2.drawStr(0, 20, data.c_str());
    u8g2.sendBuffer();
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    Serial.println();
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        displayMessage("WiFi Connected");
        delay(1500);  
        displayMessage("Scan To Unlock");
    } else {
        Serial.println("\n❌ WiFi Connection Failed!");
        displayMessage("WiFi Failed!");
    }
}

void unlockPC() {
    Serial.println(" Unlock PC...");
    delay(500);  
    Keyboard.press(KEY_F1);
    delay(100);
    Keyboard.releaseAll();
    delay(500);
    Keyboard.print("090503");
    delay(500);
    Keyboard.press(KEY_RETURN);
    delay(100);
    Keyboard.releaseAll();
}

void lockPC() {
    Serial.println("Locking PC...");
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('l');
    delay(100);
    Keyboard.releaseAll();
}

String readQRData() {
    String data = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < 1000) {
        while (qrScanner.available()) {
            char c = qrScanner.read();
            if (c == '\n') break;
            data += c;
            startTime = millis();
        }
    }
    data.trim();
    return data;
}
