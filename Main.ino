#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WebSocketsServer.h> 
#include "DHT.h"

// Replace the next variables with your SSID/Password combination
const char* ssid = "your ssid";
const char* password = "password";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "broker ip";

const char* mqtt_topic_temperature = "esp32/temperature";
const char* mqtt_topic_humidity = "esp32/humidity";
const char* mqtt_topic_distance = "esp32/distance";
const char* mqtt_topic_tilt = "esp32/tilt";

WebServer server(80); 
WebSocketsServer webSocket = WebSocketsServer(81); 
const char web[] PROGMEM = 
  R"=====(
            <!DOCTYPE HTML>
  <html>
      <head>
        
          <title>ESP32 WebSocket Server</title>
          <link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css' 
          integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'>
          <style>
            html{
              font-size: 10px;
            }
            body{
              display: inline;
          
              font-family: sans-serif;
              align-self: center;
              background: #192734;
              color: #DDE6ED;
            }
            main{
              margin-top: 1rem;
            }
            h1{
              font-size: 4rem;
              text-align: center;
              
            }
            h2{
              font-size: 2rem;
              text-align: center;
              

            }
            h2 i{

              font-size: 6rem;
              text-align: center;
            }
            .insights{
              padding: 4rem;
              display: grid;
              width: 96%;
              margin: 1 auto;
              gap: 4rem;
              grid-template-columns: repeat(3, 1fr);  
            }

            .insights div h2{
              font-size: 3rem;
            }

          </style>
      </head>
      <body>
          <h1>Chem E-Car Dashboard</h1>
          <main>
            <div class="insights">
              <div class="Temperature">
                <h2>Temperature <i class='fas fa-thermometer-half'></i></h2>
                <h1 id="Tempvalue">12</h1> 
              </div>
              <div class="Humidity">
                <h2>Humidity <i class="fa fa-tint"></i></h2>
                <h1 id="Humvalue">12</h1>
              </div>
              <div class="Velocity">
                <h2>Velocity <i class='fas fa-car'></i></h2>
                <h1 id="Velocityvalue">12</h1>
              </div>
              <div class="Tilt">
                <h2>Tilt <i class='fa fa-arrow-up'></i></h2>
                <h1 id="Tiltvalue">12</h1>
              </div>
              <div class="Distance">
                <h2>Distance <i class='fas fa-i-cursor'></i></h2>
                <h1 id="Distancevalue">12</h1>
              </div>
            </div>
          </main> 
      </body>
      <script>
        var Socket;
        function init() {
          Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
          Socket.onmessage = function(event) {
            processCommand(event);
          };
        }
        function processCommand(event) {
          var obj = JSON.parse(event.data);
          document.getElementById('Tempvalue').innerHTML = String(obj.Temp.toFixed(2)) + " deg C";
          document.getElementById('Humvalue').innerHTML = String(obj.Hum.toFixed(2)) + "%";
          document.getElementById('Velocityvalue').innerHTML = String(obj.Velocity) +" m/s";
          document.getElementById('Distancevalue').innerHTML = String(obj.Pot) +" m";
          document.getElementById('Tiltvalue').innerHTML = String(obj.Angle)+" deg";
        }
        window.onload = function(event) {
          init();
        }
      </script>
  </html>



  )=====";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


DHT dht(4, DHT22);
String jsonString;
float temperature = 0;
float humidity = 0;
float distance0 = 0;
float distance1 = 0;
float velocity =0;
float second = 0;
int angle = 0;

// push button pin
const int rightPin = 33;
const int leftPin = 32; 
const int distPin = 34;

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.print("WS Type ");
      Serial.print(type);
      Serial.println(": DISCONNECTED");
      break;
    case WStype_CONNECTED: 
      Serial.print("WS Type ");
      Serial.print(type);
      Serial.println(": CONNECTED");
      break;
  }
}

void update_webpage()
{
  StaticJsonDocument<100> doc;

  JsonObject object = doc.to<JsonObject>();
  object["Temp"] = temperature;
  object["Hum"] = humidity;
  object["Pot"] = distance0;
  object["Velocity"] = velocity;
  object["Angle"] = angle;
  object["Second"] = int(second);
  serializeJson(doc, jsonString);
  Serial.println( jsonString );
  webSocket.broadcastTXT(jsonString);
  jsonString = "";
}


class pushBtn{
  public:
    int btnPin;
    int* angle;
    bool lastState = LOW;
    bool lastBounceState = LOW;
    bool btnState;
    const int debounceDelay=50;
    unsigned long int lastDebounceTime = 0;
    int modifier;

    pushBtn(int Pin, int mod, int* deg){
      angle = deg;
      btnPin = Pin;
      modifier = mod;
    }

    void updateAngle(){
      *angle += modifier;
    }
    void buttonCheck(){
    btnState = digitalRead(btnPin);

    if((millis() - lastDebounceTime) > debounceDelay){
      if(btnState == LOW && lastState == HIGH){
        updateAngle();
      }
      lastState = btnState;
    }
    if(btnState != lastBounceState){
      lastDebounceTime = millis();
      lastBounceState = btnState;
    }
  }
};

pushBtn rightBtn(rightPin, 1, &angle);
pushBtn leftBtn(leftPin, -1, &angle);
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();


  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
    }
    else if(messageTemp == "off"){
      Serial.println("off");
    }
  }
}

void setup() {
  Serial.begin(9600);
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();
  pinMode(rightPin, INPUT_PULLUP);
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(distPin, INPUT);

  server.on("/", []() {
    server.send(200, "text\html", web);
  });
  server.begin();
  webSocket.begin(); 
  webSocket.onEvent(webSocketEvent); 
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void updateParam(){
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  distance1 = distance0;
  distance0 = analogRead(distPin)*100/4095;
  velocity = abs(distance0-distance1)/0.1;
  second += 0.5;

}

void updateMqtt(){
  char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    client.publish("esp32/temperature", tempString);

    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    client.publish("esp32/humidity", humString);

    char distString[8];
    dtostrf(distance0, 1, 2, distString);
    client.publish("esp32/distance", distString);

    char velString[8];
    dtostrf(velocity, 1, 2, velString);
    client.publish("esp32/velocity", velString);

    char angleString[8];
    dtostrf(angle, 1, 2, angleString);
    client.publish("esp32/angle", angleString);

    char timerString[8];
    dtostrf(int(second), 1, 2, timerString);
    client.publish("esp32/timer", timerString);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  server.handleClient(); 
  webSocket.loop();

  rightBtn.buttonCheck();
  leftBtn.buttonCheck();
  long now = millis();
  if (now - lastMsg > 500) {
    updateParam();
    update_webpage();
    lastMsg = now;
    updateMqtt();
    
  }
}