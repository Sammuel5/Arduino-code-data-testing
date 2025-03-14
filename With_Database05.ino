#include <Wire.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <Keyboard.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// OLED Display 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

#define RX_PIN 2  
#define TX_PIN 3  
SoftwareSerial qrScanner(RX_PIN, TX_PIN);


#define RED_LED 4    
#define GREEN_LED 5  

// WIFI & FIREBASE
#define WIFI_SSID "Shayn"
#define WIFI_PASSWORD "123456789"
#define FIREBASE_HOST "sammuel-17249-default-rtdb.firebaseio.com"
#define SERVER_ADDRESS "192.168.1.100" // server IP address

WiFiSSLClient wifiClient;
HttpClient httpClient(wifiClient, FIREBASE_HOST, 443);
// CONNECTION TO PARA SA SERVER
WiFiClient localClient;
HttpClient localHttpClient(localClient, SERVER_ADDRESS, 3000);

// DITO NAG AUTOMATED YUNG TIME PARA ACCURATE
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
#define GMT_OFFSET_SEC 8 * 3600  // ITO YUNG ORAS NG BANSA NATEN
#define NTP_UPDATE_INTERVAL_MS 60000 

// NAKA DECLARE SYA AS MONTH
const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

bool locked = true; 
bool scanComplete = false;
unsigned long lastNtpUpdate = 0;

// PAG WALANG WIFI ITO ANG GAGANA
#define INITIAL_YEAR 2025
#define INITIAL_MONTH 3
#define INITIAL_DAY 13
#define INITIAL_HOUR 3
#define INITIAL_MINUTE 11
#define INITIAL_SECOND 0

void displayMessage(const char *message);
void displayQRData(String data);
void sendDataToFirebase(String qrData, bool isTimeIn);
void sendDataToLocalServer(String qrData, bool isTimeIn);
String readQRData();
void resetLEDs();
void unlockPC();
void lockPC();
void connectToWiFi();
String getFormattedDate();
String getFormattedTime();
void setupTime();
void updateTimeFromNTP();

// IMPLEMENT TO BAGO GAMITIN YUNG MGA FUNCTION
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

String getFormattedDate() {
    // Format date "Mmm dd yyyy" ( "Mar 14 2025")
    char dateBuffer[12];
    sprintf(dateBuffer, "%s %d %04d", 
            monthNames[month() - 1], day(), year());
    return String(dateBuffer);
}

String getFormattedTime() {
    // Format time "hh:mm:ss" ("14:30:45") MILITARY TIME
    char timeBuffer[9];
    sprintf(timeBuffer, "%02d:%02d:%02d", 
            hour(), minute(), second());
    return String(timeBuffer);
}

void resetLEDs() {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
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

void setupTime() {
    // KITA MO YUNG SA TAAS ANO NAKALAGAY? DIBA SETUP TIME
    setTime(INITIAL_HOUR, INITIAL_MINUTE, INITIAL_SECOND, INITIAL_DAY, INITIAL_MONTH, INITIAL_YEAR);
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (attempts < 30) {  // Increased timeout
        delay(1000);
        Serial.print(".");
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ WiFi Connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            
            // Check if IP is valid
            if (WiFi.localIP()[0] != 0) {
                displayMessage("WiFi Connected");
                delay(1500);
                displayMessage("Scan To Unlock");
                return;
            } else {
                Serial.println("Invalid IP address. Waiting for valid IP...");
            }
        }
        attempts++;
    }
    
    Serial.println("\n❌ WiFi Connection Failed!");
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    displayMessage("WiFi Failed!");
}

void updateTimeFromNTP() {
    Serial.println("Updating time from NTP server...");
    displayMessage("Syncing time...");
    
    if (timeClient.update()) {
        // Get full date and time from NTP
        unsigned long epochTime = timeClient.getEpochTime();
        
        // Convert to time_t and sync with TimeLib
        setTime(epochTime);
        
        Serial.println("✅ NTP time sync successful!");
        Serial.println("Current time: " + getFormattedDate() + " " + getFormattedTime());
        displayMessage("Time synced!");
        delay(1000);
        displayMessage(locked ? "Scan To Unlock" : "Scan To Lock");
        lastNtpUpdate = millis();
    } else {
        Serial.println("❌ NTP time sync failed!");
        displayMessage("Time sync failed");
        delay(1000);
        displayMessage(locked ? "Scan To Unlock" : "Scan To Lock");
    }
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
        Serial.println("✅ Data sent successfully to Firebase!");
        
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
        Serial.println("❌ Failed to send data to Firebase!");
    }
}

void sendDataToLocalServer(String qrData, bool isTimeIn) {
    // Get current date and time at the moment of scanning
    String formattedDate = getFormattedDate();
    String formattedTime = getFormattedTime();
    
    // Create a path for the QR code
    String path = "/send";
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

    Serial.println("Sending data to local server...");
    Serial.println("Server: " + String(SERVER_ADDRESS) + ":3000");
    Serial.println("Data: " + jsonData);

    localHttpClient.setTimeout(5000);
    localHttpClient.beginRequest();
    localHttpClient.post(path);
    localHttpClient.sendHeader("Content-Type", "application/json");
    localHttpClient.sendHeader("Content-Length", jsonData.length());
    localHttpClient.beginBody();
    localHttpClient.print(jsonData);
    localHttpClient.endRequest();

    int statusCode = localHttpClient.responseStatusCode();
    String response = localHttpClient.responseBody();

    Serial.print("HTTP Status Code: ");
    Serial.println(statusCode);
    Serial.println("Server Response: " + response);

    if (statusCode == 200) {
        Serial.println("✅ Data sent successfully to local server!");
    } else {
        Serial.println("❌ Failed to send data to local server!");
    }
}

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

    // Use initial time as fallback
    setupTime();
    Serial.println("Initial time set to: " + getFormattedDate() + " " + getFormattedTime());

    digitalWrite(RED_LED, HIGH);
    displayMessage("System Locked");
    delay(1500);

    connectToWiFi();
    
    // Initialize and update NTP client after WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.begin();
        timeClient.setTimeOffset(GMT_OFFSET_SEC);
        updateTimeFromNTP();
    }
}

void loop() {
    // Check if it's time to update from NTP
    if (WiFi.status() == WL_CONNECTED && (millis() - lastNtpUpdate > NTP_UPDATE_INTERVAL_MS)) {
        updateTimeFromNTP();
    }
    
    if (qrScanner.available() && !scanComplete) {
        String qrData = readQRData();

        if (qrData.length() > 0) {
            if (qrData.length() == 11 && qrData.startsWith("UA") && qrData.substring(2).toInt() > 0) {  
                scanComplete = true;  
                Serial.println("QR Data: " + qrData);
                displayQRData(qrData);
                
                if (locked) {  
                    // This is a time-in scan
                    sendDataToFirebase(qrData, true);
                    sendDataToLocalServer(qrData, true);
                    
                    unlockPC();
                    digitalWrite(GREEN_LED, HIGH);
                    digitalWrite(RED_LED, LOW);
                    locked = false;
                    displayMessage("Scan To Lock");  
                } else {  
                    // This is a time-out scan
                    sendDataToFirebase(qrData, false);
                    sendDataToLocalServer(qrData, false);
                    
                    lockPC();
                    digitalWrite(RED_LED, HIGH);
                    digitalWrite(GREEN_LED, LOW);
                    locked = true;
                    displayMessage("Scan To Unlock");  
                }

                delay(2000);  
                scanComplete = false;  
            } else {
                // Invalid QR code format
                Serial.println("Invalid QR code format: " + qrData);
                displayMessage("Invalid QR Code");
                delay(2000);
                displayMessage(locked ? "Scan To Unlock" : "Scan To Lock");
            }
        }
    }
}
