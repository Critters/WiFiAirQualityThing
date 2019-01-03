// WIFI
#include <ESP8266WiFi.h>
#include                "Adafruit_MQTT.h"
#include                "Adafruit_MQTT_Client.h"
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define WLAN_SSID       "<<WIFI SSID>>"
#define WLAN_PASS       "<<WIFI PASSWORD>>"
#define IO_USERNAME     "<<ADAFRUIT IO USRNAME>>"
#define IO_KEY          "<<ADAFRUIT IO KEY>>"
#define GROUP_KEY       "<<LOCATION NAME>>"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);
Adafruit_MQTT_Publish mqttParticles = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".particles");
Adafruit_MQTT_Publish mqttPM25 = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".pm25");
Adafruit_MQTT_Publish mqttPM100 = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".pm100");
void MQTT_connect();

// We store 10 values in an array then, once we have filled the array, we average them
int particleArray[10];    // Particle count
int PM25Array[10];        // PM2.5 count
int PM100Array[100];      // PM10 count
int arrayPosition = 0;    // Where in the array we are writing
int particleAverage, PM25Average, PM100Average;   // Used for averaging before sending data to AdafruitIO

// SENSOR
#include <SoftwareSerial.h>
SoftwareSerial pmsSerial(13, 7);

// The sensor data is stored here, there is a lot more you could send if you want to expand this beyond particle count and size
// Maybe add a temperature / humidity sensor? That'd be cool
struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};
struct pms5003data data;
 
void setup() {
  Serial.begin(115200);
  pmsSerial.begin(9600);

  // WIFI
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  
  Serial.println("Started");
}


void loop() {
  if(readPMSdata(&pmsSerial)) {
      Serial.print(data.pm25_env); Serial.print(","); 
      Serial.print(data.pm100_env); Serial.print(","); 
      Serial.print(data.particles_03um); 
      Serial.print("\n");
      
      if(arrayPosition < 10){
          particleArray[arrayPosition] = data.particles_03um;
          PM25Array[arrayPosition] = data.pm25_env;
          PM100Array[arrayPosition] = data.pm100_env;
          arrayPosition++;
      }else{
          particleAverage = 0;
          PM25Average = 0;
          PM100Average = 0;
          for(int i = 0; i<10; i++){
              particleAverage += particleArray[i];
              PM25Average += PM25Array[i];
              PM100Average += PM100Array[i];
          }
          MQTT_connect();
          mqttParticles.publish(particleAverage/10);
          mqttPM25.publish(PM25Average/10);
          mqttPM100.publish(PM100Average/10);
          
          arrayPosition = 0;

          Serial.print("AVERAGE: ");
          Serial.print(PM25Average/10); Serial.print(","); 
          Serial.print(PM100Average/10); Serial.print(","); 
          Serial.print(particleAverage/10); 
          Serial.print("\n");
      }

      delay(2000);
  }
}
 
boolean readPMSdata(Stream *s) {
  if (! s->available()) {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != 0x42) {
    s->read();
    return false;
  }
 
  // Now read all 32 bytes
  if (s->available() < 32) {
    return false;
  }
    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  s->readBytes(buffer, 32);
 
  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }
  
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
 
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);
 
  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    return false;
  }
  // success!
  return true;
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}