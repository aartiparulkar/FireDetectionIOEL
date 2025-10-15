#include <DNSServer.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>


#define SMOKE_PIN D5
#define FLAME_PIN D6
#define DHTPIN D2
#define DHTTYPE DHT11
#define Red_led D4
#define Green_led D8

DHT dht(DHTPIN, DHTTYPE);

/* --------------------- WiFi Credentials and WiFi status ----------------*/
const char *ssid = "";      //Your WiFi Router SSID
const char *password = "";   // Your WiFi Router password
int status = WL_IDLE_STATUS;             // Connection status


/* ---------------------- API credentials and SMS Details ---------------------- */
const char* apiKey = "";          // Replace with your API key
const char* templateID = "101";           // Replace with your template ID
const char* mobileNumber = ""; // Replace with the recipient's mobile number with country code (eg : 91XXXXXXXXXX)
const char* var1 = "house";   // Replace with your custom variable
const char* var2 = "FIRE EMERGENCY. Evacuate now!";          // Replace with your custom variable

/* --------------------------- WiFi connect Function --------------------------- */
void wifi_connect() {
  Serial.print("Connecting to Wifi");
  WiFi.begin(ssid, password);

  int retries = 0;
  
  while(WiFi.status() != WL_CONNECTED ) {
    delay(1000);
    Serial.print(".");
    retries++;

    if (retries > 30) { // ~30 seconds
      Serial.println("\nFailed to connect. Restarting...");
      ESP.restart();
    }
  } 
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

/* ------------------------ WiFi reconnect Function ----------------------------- */
void wifi_reconnect(){
   Serial.println("Wifi Reconnecting........");
   delay(6000);
   wifi_connect();
}

/* ------------------------ SMS Trigger Function ---------------------------------- */
void trigger_SMS(){
  if (WiFi.status() == WL_CONNECTED) {
   WiFiClient client;                                     // Initialize WiFi client

   String apiUrl = "/send_sms?ID=" + String(templateID);
   
   Serial.print("Connecting to server...");
   
   if (client.connect("www.circuitdigest.cloud", 80)) {   // Connect to the server
     Serial.println("connected!");
     // Create the HTTP POST request
     String payload = "{\"mobiles\":\"" + String(mobileNumber) +
                      "\",\"var1\":\"" + String(var1) +
                      "\",\"var2\":\"" + String(var2) + "\"}";
   
     // Send HTTP request headers
     client.println("POST " + apiUrl + " HTTP/1.1");
     client.println("Host: www.circuitdigest.cloud");
     client.println("Authorization: " + String(apiKey));
     client.println("Content-Type: application/json");
     client.println("Content-Length: " + String(payload.length()));
     client.println();                                    // End of headers
     client.println(payload);                             // Send the JSON payload
   
     // Wait for the response
     int responseCode = -1; // Variable to store HTTP response code
   
     while (client.connected() || client.available()) {
       if (client.available()) {
         String line = client.readStringUntil('\n'); // Read a line from the response
         Serial.println(line); // Print the response line (for debugging)
   
         // Check for the HTTP response code
         if (line.startsWith("HTTP/")) {
           responseCode = line.substring(9, 12).toInt(); // Extract response code (e.g., 200, 404)
           Serial.print("HTTP Response Code: ");
           Serial.println(responseCode);
         }
   
         // Stop reading headers once we reach an empty line
         if (line == "\r") {
           break;
         }
       }
     }
     // Check response
     if (responseCode == 200) {
       Serial.println("SMS sent successfully!");
     } else {
       Serial.print("Failed to send SMS. Error code: ");
       Serial.println(responseCode);
     }
     client.stop(); // Disconnect from the server
   } else {
     Serial.println("Connection to server failed!");
   }
 } else {
   Serial.println("WiFi not connected!");
 }
}

bool fire_smoke_detect() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 2000) return false; // wait 2s between reads
  lastRead = millis();

  // Read smoke & flame sensors
  int smokeValue = digitalRead(SMOKE_PIN);
  int flameValue = digitalRead(FLAME_PIN);

  // Read DHT sensor with retries
  float temperature = NAN;
  float humidity = NAN;
  const int maxRetries = 5;

  for (int i = 0; i < maxRetries; i++) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity)) break; // successful read
    delay(200); // short delay before retry
  }

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor after retries!");
    return false;
  }

  // Print sensor values
  Serial.println("---- Sensor Data ----");
  Serial.print("Smoke: "); Serial.println(smokeValue);
  Serial.print("Flame: "); Serial.println(flameValue);
  Serial.print("Temperature: "); Serial.println(temperature);
  Serial.print("Humidity: "); Serial.println(humidity);
  Serial.println("----------------------");

  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Reconnecting...");
    wifi_reconnect();
  }

  // HTTP POST to Flask
  HTTPClient http;
  WiFiClient client;
  String serverUrl = "http://your-ip:5000/predict";


  StaticJsonDocument<256> jsonDoc;
  jsonDoc["smoke"] = smokeValue;
  jsonDoc["flame"] = flameValue;
  jsonDoc["temperature"] = temperature;
  jsonDoc["humidity"] = humidity;

  String requestBody;
  serializeJson(jsonDoc, requestBody);

  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server Response: " + response);

    StaticJsonDocument<256> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error) {
      String prediction = responseDoc["prediction"].as<String>();

      if (prediction == "fire") {
        Serial.println("🔥 Fire Detected! Turning on RED LED.");
        digitalWrite(Red_led, HIGH);
        digitalWrite(Green_led, LOW);
        trigger_SMS(); // Send alert
        http.end();
        return true;
      } else {
        Serial.println("✅ Safe. Turning on GREEN LED.");
        digitalWrite(Red_led, LOW);
        digitalWrite(Green_led, HIGH);
      }
    } else {
      Serial.println("Error parsing server response");
    }
  } else {
    Serial.print("Error sending data: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return false;
}




/* ------------------------- Setup Function --------------------------------*/
void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial);

  // Initialize DHT sensor
  dht.begin();

  wifi_connect();

  // Initialize pins
  pinMode(SMOKE_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(Green_led, OUTPUT);
  pinMode(Red_led, OUTPUT);


  digitalWrite(Green_led, HIGH);
  digitalWrite(Red_led, LOW);

  Serial.println("Setup complete, sensors ready!");    
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WiFi.status() != WL_CONNECTED){
       wifi_reconnect();
   }

   fire_smoke_detect();
   delay(5000);
}
