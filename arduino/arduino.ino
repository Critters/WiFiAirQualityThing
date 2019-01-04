#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ESP8266WiFi.h>

// VARIABLES
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define WLAN_SSID       "<<WIFI SSID>>"
#define WLAN_PASS       "<<WIFI PASSWORD>>"
#define IO_USERNAME     "<<ADAFRUIT IO USRNAME>>"
#define IO_KEY          "<<ADAFRUIT IO KEY>>"
#define GROUP_KEY       "<<LOCATION NAME>>"
#define PIN_DATA        7 // RX from pin 5 on the sensor

/* READ_DELAY: Seconds between readind the sensor
 * if >=60 then sensor will sleep between readings but will wake up 30 seconds before taking a reading (as recommended in the manual)
 * if <60 then the sensor will always be on, you also don't need to wire "PIN_RESET" if you intend to keep it this way */
#define READ_DELAY      30

/* SAMPLE_COUNT: Nnumber of samples to take and average before sending data to Adafruit.IO
 * Therefore, (READ_DELAY * SAMPLE_COUNT) = Time between sending data 
 * NOTE: this is not going to be super accurate, the sensor takes some amount of time to read, the data some amount of time to send, sometimes the sensor data fails the checksum, but it's close enough*/
#define SAMPLE_COUNT    10

// WIFI
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);
Adafruit_MQTT_Publish mqttParticles = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".particles");
Adafruit_MQTT_Publish mqttPM25 = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".pm25");
Adafruit_MQTT_Publish mqttPM100 = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/" GROUP_KEY ".pm100");
void MQTT_connect();

// We store SAMPLE_COUNT values in an array then, once we have filled the array, we average them
int particleArray[SAMPLE_COUNT];  // Particle count
int PM25Array[SAMPLE_COUNT];      // PM2.5 count
int PM100Array[SAMPLE_COUNT];     // PM10 count
int arrayPosition = 0;            // Where in the array we are writing
int particleAverage, PM25Average, PM100Average;   // Used for averaging before sending data to AdafruitIO

// SENSOR
#include <SoftwareSerial.h>
SoftwareSerial pmsSerial(13, PIN_DATA);

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
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  MQTT_connect();
  
  Serial.print("\nSampling every "); Serial.print(READ_DELAY); Serial.print(" seconds, and send data every "); Serial.print((SAMPLE_COUNT*READ_DELAY)/60); Serial.print(" minutes (averaging "); Serial.print(SAMPLE_COUNT); Serial.println(" values)");
}


void loop() {
  if(readPMSdata(&pmsSerial)) {
      Serial.print(arrayPosition+1); Serial.print("/"); Serial.print(SAMPLE_COUNT); Serial.print(": ");
      Serial.print("PM2.5: "); Serial.print(data.pm25_env); Serial.print(", "); 
      Serial.print("PM10: "); Serial.print(data.pm100_env); Serial.print(", "); 
      Serial.print("Particles: "); Serial.print(data.particles_03um); 
      Serial.print("\n");

      particleArray[arrayPosition] = data.particles_03um;
      PM25Array[arrayPosition] = data.pm25_env;
      PM100Array[arrayPosition] = data.pm100_env;
      arrayPosition++;
      if(arrayPosition == SAMPLE_COUNT){
          particleAverage = 0;
          PM25Average = 0;
          PM100Average = 0;
          for(int i = 0; i<SAMPLE_COUNT; i++){
              particleAverage += particleArray[i];
              PM25Average += PM25Array[i];
              PM100Average += PM100Array[i];
          }
          MQTT_connect();
          mqttParticles.publish(particleAverage/SAMPLE_COUNT);
          mqttPM25.publish(PM25Average/SAMPLE_COUNT);
          mqttPM100.publish(PM100Average/SAMPLE_COUNT);
          
          arrayPosition = 0;

          Serial.print("SENT AVERAGE: ");
          Serial.print(PM25Average/SAMPLE_COUNT); Serial.print(","); 
          Serial.print(PM100Average/SAMPLE_COUNT); Serial.print(","); 
          Serial.print(particleAverage/SAMPLE_COUNT); 
          Serial.print("\n");
      }

      delay(READ_DELAY*1000);
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
    //Serial.println("Checksum failure");
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
