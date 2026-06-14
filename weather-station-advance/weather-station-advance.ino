#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_HTU21DF.h>
#include <Adafruit_BMP280.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>

#define LIGHT_ANALOG_PIN 33
#define POTENTIOMETER_PIN 35
#define BUZZER_PIN 16

const char* ssid = "your_wifi_ssid";        
const char* password = "your_wifi_password"; 

Adafruit_HTU21DF htu = Adafruit_HTU21DF();
Adafruit_BMP280 bmp; 

WebServer server(80);

int currentPage = 1;  
int lastPage = 1;  
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000; 
unsigned long startTime = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);


struct SensorData {
  float temperature;
  float humidity;
  float pressure;
  int light;
  unsigned long realTime; 
};

const int maxDataPoints = 24; 
SensorData dataHistory[maxDataPoints];
int dataIndex = 0;
int dataCount = 0;

void setup() {
  Serial.begin(115200);

  startTime = millis();
  Wire.begin(21, 22);

  if (!htu.begin()) {
    Serial.println("HTU21D not found!");
  } else {
    Serial.println("HTU21D initialized");
  }
  

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found at address 0x76!");

  }
  
 
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                  Adafruit_BMP280::SAMPLING_X2,     
                  Adafruit_BMP280::SAMPLING_X16,    
                  Adafruit_BMP280::FILTER_X16,    
                  Adafruit_BMP280::STANDBY_MS_500); 
  

  
  pinMode(POTENTIOMETER_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("Hello Sir!");
  

  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
    
   
    setupWebServer();
    server.begin();
    Serial.println("HTTP server started");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    delay(2000);
  }

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
  Serial.print("Waiting for NTP time...");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println(" done!");





}


float getLightLevel() {
  int lightLevel = analogRead(LIGHT_ANALOG_PIN);
  lightLevel = map(lightLevel, 0, 4095, 0, 100);
  return 100 - lightLevel;
}


void setupWebServer() {
  
  server.on("/", HTTP_GET, []() {
    String html = generateHTMLPage();
    server.send(200, "text/html", html);
  });

  server.on("/api/data", HTTP_GET, []() {
    StaticJsonDocument<200> doc;
    doc["temperature"] = htu.readTemperature();
    doc["humidity"] = htu.readHumidity();
    doc["pressure"] = bmp.readPressure() / 133.322;
    doc["light"] = getLightLevel();
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
server.on("/api/history", HTTP_GET, []() {
    DynamicJsonDocument doc(8192);
    JsonArray temps = doc.createNestedArray("temperatures");
    JsonArray humidities = doc.createNestedArray("humidities");
    JsonArray pressures = doc.createNestedArray("pressures");
    JsonArray lights = doc.createNestedArray("lights");
    JsonArray times = doc.createNestedArray("times");

    
    int startIndex;
    if (dataCount < maxDataPoints) {
        startIndex = 0;                 
    } else {
        startIndex = dataIndex;         
    }

    for (int i = 0; i < dataCount; i++) {
        int idx = (startIndex + i) % maxDataPoints;
        temps.add(dataHistory[idx].temperature);
        humidities.add(dataHistory[idx].humidity);
        pressures.add(dataHistory[idx].pressure);
        lights.add(dataHistory[idx].light);
        times.add(dataHistory[idx].realTime); 
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
});
}

String generateHTMLPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Weather Station</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        h1 {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        .current-data {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            text-align: center;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            transition: transform 0.3s;
        }
        .card:hover {
            transform: translateY(-5px);
        }
        .card h3 {
            margin: 0 0 10px 0;
            color: #667eea;
        }
        .card .value {
            font-size: 2em;
            font-weight: bold;
            margin: 10px 0;
        }
        .card .unit {
            font-size: 0.8em;
            color: #666;
        }
        .chart-container {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        canvas {
            max-height: 300px;
        }
        .refresh-info {
            text-align: center;
            color: white;
            margin-top: 20px;
            font-size: 0.9em;
        }
        @media (max-width: 768px) {
            .current-data {
                grid-template-columns: repeat(2, 1fr);
            }
            .card .value {
                font-size: 1.5em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Weather Station</h1>
        
        <div class="current-data">
            <div class="card">
                <h3>Temperature</h3>
                <div class="value" id="temp">--</div>
                <div class="unit">°C</div>
            </div>
            <div class="card">
                <h3>Humidity</h3>
                <div class="value" id="hum">--</div>
                <div class="unit">%</div>
            </div>
            <div class="card">
                <h3>Pressure</h3>
                <div class="value" id="press">--</div>
                <div class="unit">mmHg</div>
            </div>
            <div class="card">
                <h3>Light</h3>
                <div class="value" id="light">--</div>
                <div class="unit">%</div>
            </div>
        </div>
        
        <div class="chart-container">
            <canvas id="tempChart"></canvas>
        </div>
        <div class="chart-container">
            <canvas id="humChart"></canvas>
        </div>
        <div class="chart-container">
            <canvas id="pressChart"></canvas>
        </div>
        <div class="chart-container">
            <canvas id="lightChart"></canvas>
        </div>
        
        <div class="refresh-info">
            Data updates every hour | Last update: <span id="lastUpdate">--:--:--</span>
        </div>
    </div>

    <script>
        let tempChart, humChart, pressChart, lightChart;
        
        function initCharts() {
            const ctxTemp = document.getElementById('tempChart').getContext('2d');
            const ctxHum = document.getElementById('humChart').getContext('2d');
            const ctxPress = document.getElementById('pressChart').getContext('2d');
            const ctxLight = document.getElementById('lightChart').getContext('2d');
            
            tempChart = new Chart(ctxTemp, {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Temperature (°C)', data: [], borderColor: 'rgb(255, 99, 132)', backgroundColor: 'rgba(255, 99, 132, 0.1)', tension: 0.4 }] },
                options: { responsive: true, maintainAspectRatio: true, plugins: { legend: { position: 'top' } } }
            });
            
            humChart = new Chart(ctxHum, {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Humidity (%)', data: [], borderColor: 'rgb(54, 162, 235)', backgroundColor: 'rgba(54, 162, 235, 0.1)', tension: 0.4 }] },
                options: { responsive: true, maintainAspectRatio: true, plugins: { legend: { position: 'top' } } }
            });
            
            pressChart = new Chart(ctxPress, {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Pressure (mmHg)', data: [], borderColor: 'rgb(75, 192, 192)', backgroundColor: 'rgba(75, 192, 192, 0.1)', tension: 0.4 }] },
                options: { responsive: true, maintainAspectRatio: true, plugins: { legend: { position: 'top' } } }
            });
            
            lightChart = new Chart(ctxLight, {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Light Level (%)', data: [], borderColor: 'rgb(255, 159, 64)', backgroundColor: 'rgba(255, 159, 64, 0.1)', tension: 0.4 }] },
                options: { responsive: true, maintainAspectRatio: true, plugins: { legend: { position: 'top' } } }
            });
        }
        
        async function fetchCurrentData() {
            try {
                const response = await fetch('/api/data');
                const data = await response.json();
                
                document.getElementById('temp').textContent = data.temperature.toFixed(1);
                document.getElementById('hum').textContent = data.humidity.toFixed(1);
                document.getElementById('press').textContent = data.pressure.toFixed(1);
                document.getElementById('light').textContent = data.light;
                
                const now = new Date();
                document.getElementById('lastUpdate').textContent = now.toLocaleTimeString();
            } catch (error) {
                console.error('Error fetching current data:', error);
            }
        }
        
        async function fetchHistoryData() {
          try {
            const response = await fetch('/api/history');
            const data = await response.json();
            
            // Преобразуем время из миллисекунд в читаемый формат
            const times = data.times.map(timestamp => {
  const date = new Date(timestamp * 1000); // Конвертируем Unix timestamp в миллисекунды
  return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
});
            
            tempChart.data.labels = times;
            tempChart.data.datasets[0].data = data.temperatures;
            tempChart.update();
            
            humChart.data.labels = times;
            humChart.data.datasets[0].data = data.humidities;
            humChart.update();
            
            pressChart.data.labels = times;
            pressChart.data.datasets[0].data = data.pressures;
            pressChart.update();
            
            lightChart.data.labels = times;
            lightChart.data.datasets[0].data = data.lights;
            lightChart.update();
          } catch (error) {
            console.error('Error fetching history data:', error);
          }
        }
        
        function updateData() {
            fetchCurrentData();
            fetchHistoryData();
        }
        
        initCharts();
        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
  )rawliteral";
  return html;
}

void storeData(float temp, float humidity, float pressure, int light) {
  dataHistory[dataIndex].temperature = temp;
  dataHistory[dataIndex].humidity = humidity;
  dataHistory[dataIndex].pressure = pressure;
  dataHistory[dataIndex].light = light;
  dataHistory[dataIndex].realTime = time(nullptr);
  
  dataIndex = (dataIndex + 1) % maxDataPoints;
  if (dataCount < maxDataPoints) {
    dataCount++;
  }
}

void playBeep() {
  tone(BUZZER_PIN, 523, 150);  
  delay(200);
  tone(BUZZER_PIN, 698, 150);  
  delay(200);
  noTone(BUZZER_PIN);
}

void loop() {
  server.handleClient(); 
  
  float temp = htu.readTemperature();
  float humidity = htu.readHumidity();
  float pressure = bmp.readPressure() / 133.322; 
  float lightLevel = getLightLevel();

  int sensorValue = analogRead(POTENTIOMETER_PIN);
  sensorValue = map(sensorValue, 0, 4095, 0, 1023);

  if (sensorValue >= 0 && sensorValue < 205) {
    currentPage = 1;
  } else if (sensorValue >= 205 && sensorValue < 410) {
    currentPage = 2;
  } else if (sensorValue >= 410 && sensorValue < 615) {
    currentPage = 3;
  } else if (sensorValue >= 615 && sensorValue < 820) {
    currentPage = 4;
  } else {
    currentPage = 5;
  }

  if (currentPage != lastPage) {
    playBeep();
    lastPage = currentPage;
  }


  if (currentPage == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Meteorological");
    lcd.setCursor(0, 1);
    lcd.print("    station");
  } else if (currentPage == 2) {
    char tempStr[10];
    sprintf(tempStr, "%02d C", (int)temp);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temperature ");
    lcd.setCursor(0, 1);
    lcd.print(tempStr);
  } 
  else if (currentPage == 3) {
    char HumStr[10];
    sprintf(HumStr, "%02d%%", (int)humidity);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Humidity ");
    lcd.setCursor(0, 1);
    lcd.print(HumStr);
  } 
  else if (currentPage == 4) {
    char pressureStr[16];
    sprintf(pressureStr, "%.1f mmHg", pressure);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pressure: ");
    lcd.setCursor(0, 1);
    lcd.print(pressureStr);
  } else if (currentPage == 5) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Lightness: ");
    lcd.setCursor(0, 1);
    lcd.print((int)lightLevel);
    lcd.print(" %");
  } 
  
  static unsigned long lastStoreTime = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastStoreTime >= 3600000 || lastStoreTime == 0) {
      storeData(temp, humidity, pressure, (int)lightLevel);
      lastStoreTime = currentMillis;
  }
  delay(100);
}
