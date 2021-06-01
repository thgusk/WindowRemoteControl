#include <MQ7.h>
#include <Stepper.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQ7.h>

#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ7PIN 34


const char* ssid = "s301";
const char* password = "01200120"; 

AsyncWebServer server(80);

AsyncEventSource events("/events");

unsigned long lastTime = 0;  
unsigned long timerDelay = 10000;

boolean window = false;  //창문 개폐여부(열림=true, 닫힘=false)
String Window;
DHT dht(DHTPIN, DHTTYPE);;  //온도센서
int Raindrops_pin = 33;  //빗물센서
int co_pin = 34;  //일산화탄소센서
MQ7 mq7(co_pin, 5.0); 
//int co2_pin = ;  //이산화탄소센서
const int stepsPerRevolution = 1024;  // 2048:한바퀴(360도), 1024:반바퀴(180도)...
Stepper myStepper(stepsPerRevolution,23,21,22,19);  // 모터 드라이브에 연결된 핀 IN4, IN2, IN3, IN1


float co;
float temperature;
float humidity;

///////////////////////////////////////////////////////////////


void w_Open(){  //센서값에 의해 open할때
  if(window == false){
    myStepper.step(stepsPerRevolution);
    window = true;
  }
}

void w_Close(){  //센서값에 의해 close할때
  if(window == true){
    myStepper.step(-stepsPerRevolution);
    window = false;
  }
}


void getSensorReadings(){
  co = mq7.getPPM();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());
}

String processor(const String& var){
  getSensorReadings();
  Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  else if(var == "CO"){
    return String(co);
  }
  else if(var == "WINDOW"){
    if(window==true){
      return String("열림");
    }else if(window==false){
      return String("닫힘");
    }
  }
  return String();
}


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
    .font1 {font-size: 2rem; color: blue;}
  </style>
</head>

<body>
  <div class="content">
    <div class="cards">
      <div>
        <p>창문은 지금  <span class="font1"><span id="window">%WINDOW%</span></span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> TEMPERATURE</p><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-tint" style="color:#00add6;"></i> HUMIDITY</p><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e437;"></i> CO</p><p><span class="reading"><span id="co">%CO%</span> ppm</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e400;"></i> CO2</p><p><span class="reading"><span id="co2">%CO2%</span> ppm</span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('co', function(e) {
  console.log("CO", e.data);
  document.getElementById("co").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";

///////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  initWiFi();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
  dht.begin();  


  server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(window == false){
      myStepper.step(stepsPerRevolution);
      window = true;
      }
  });

  server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(window == true){
      myStepper.step(-stepsPerRevolution);
      window = false;
      }
  });

  
  pinMode(Raindrops_pin , INPUT);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  
  window = true;
  myStepper.setSpeed(60);
  //myStepper.step(0);
}


void loop() {
  if ((millis() - lastTime) > timerDelay) {
    getSensorReadings();
    Serial.printf("Temperature = %.2f ºC \n", temperature);
    Serial.printf("Humidity = %.2f \n", humidity);
    Serial.printf("co = %.2f hPa \n", co);
    Serial.println();

    // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(humidity).c_str(),"humidity",millis());
    events.send(String(co).c_str(),"CO",millis());
    
    lastTime = millis();
  }
  

  Serial.print("온도 : ");
  Serial.print(temperature);
  Serial.println("ºC");
  Serial.print("습도 : ");
  Serial.print(humidity);
  Serial.println("%");
  
  // 빗물센서
  int raindropSensor = analogRead(Raindrops_pin);
  Serial.printf("raindrop: %d\n", raindropSensor);
  
  // 일산화탄소센서
  Serial.printf("CO: %.2f ppm\n", co);

  // 이산화탄소센서 
  //int co2sensor1 = analogRead();
  //int co2sensor2 = analogRead();

  delay(500);

  //비올때
  if(raindropSensor < 2500 && window == true){  //평상시 약 3300
    w_Close(); //닫힘
  }
  
  //일산화탄소 감지시
  if(co > 50 && window == false){
    w_Open(); //열림
  }
  
}
