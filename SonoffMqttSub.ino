//https://www.instructables.com/id/MQTT-Bare-Minimum-Sketch/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <stdlib.h>

#define wifi_ssid "jomilida24"
#define wifi_password "jomilida"

// If this hardware is a thermostate, set the heater control output to RELAY_CONTROL.
// If NOT a heater control, set the heater control output to 0.
#define HEATER_CONTROL RELAY_CONTROL
//Similarly, if this is a motion light, set the MOTION_SENSOR_LIGHT_CONTROL 
#define MOTION_SENSOR_LIGHT_CONTROL 0
#define mqtt_server "192.168.1.1"
#define mqtt_port 1883
#define mqtt_user ""
#define mqtt_password ""
#define ESP8266_GPIO2  2
#define ESP8266_GPIO4  4
#define ESP8266_GPIO12 12
#define ESP8266_GPIO13 13
#define RELAY_CONTROL ESP8266_GPIO12
#define LED           ESP8266_GPIO13
#define in_topic "jomilida"
#define delimProperty '^'
#define delimMeasure ':'

#define ON_SENSOR_LIGHT 1
#define LEAVE_ON_FOR_SECONDS 10
#define TYPE_TEMP 1
#define TYPE_MOTION 2
#define TYPE_SET_HEATER_TARGET_TEMP 3
#define UNITS_TENTHS_DEG_C 1
#define UNITS_PERCENTAGE 2

WiFiClient espClient;
PubSubClient client;
unsigned long lastMotion = 0;// millis() time the last time we are condifdent motion occured
bool motionSensorRelayStatus;
int heaterTargetdC=215;
bool heaterControlStatus = false;


#define ACTION_ON 1
#define ACTION_OFF 2
struct deviceActionStruct {
  int device;
  int action;
};

struct deviceActionStruct* parseMsg(String msg) {
  bool debug=true;
  unsigned int pos=0;
  unsigned int len = msg.length();
  unsigned int sensorTypeStart = 0;
  unsigned int sensorTypeEnd;
  char measureTypeChar;
  int measureType = 0;
  long val;
  char durationType;
  unsigned int durationVal;
  unsigned int valUnits = 0;
  unsigned long durationSeconds;
  unsigned int number;
  char tempChar;
 
  struct deviceActionStruct * retval=NULL;
  if (debug) {
    Serial.print("Parsing ");
    Serial.println(msg);
  }
  if (len==0) 
  }
  while(pos<len) {
    
    measureType = 0;
    valUnits = 0;
    // first get what is being measured (temp or motion) and units. Only care about the stuff this device responds to, so if additional physical properties are being measured we can just skip them.:
    measureTypeChar = msg.charAt(pos);
    Serial.printf("Welcome to the main parse loop. pos=%d which is '%c'\n",pos,measureTypeChar);
    
    if ('T' == measureTypeChar) {
      measureType = TYPE_TEMP;
      if ((len > pos+2) && 'd' == msg.charAt(pos+1) && 'C' == msg.charAt(pos+2)) {
        // values are tenths of degrees celcius so 221 = 22.1degC. 'd' is the SI prefix for tenths
        valUnits = UNITS_TENTHS_DEG_C;
        pos = pos +3;
      }
    }
    if ('M' == measureTypeChar) {
      measureType = TYPE_MOTION;
      if ((len > pos+1) && msg.charAt(pos+1) == '%') {
        valUnits = UNITS_PERCENTAGE;
        pos = pos + 2;
      }
    }
    if ('H' == measureTypeChar) {
      measureType = TYPE_SET_HEATER_TARGET_TEMP;
      if ((len > pos+2) && 'd' == msg.charAt(pos+1) && 'C' == msg.charAt(pos+2)) {
        // values are tenths of degrees celcius so 221 = 22.1degC. 'd' is the SI prefix for tenths
        valUnits = UNITS_TENTHS_DEG_C;
        pos = pos +3;
      }
    }
    if (debug && measureType == 0) {
      Serial.printf("Invalid measure type '%c'\n",measureTypeChar);
    }
    if (debug) {
      tempChar = msg.charAt(pos);
      Serial.printf("val measureType= %d, units = %d, pos = %d, next char = %c\n",measureType, valUnits,pos,tempChar);
    }
    if (msg.charAt(pos) == delimMeasure) pos++;
    if (measureType && valUnits) {
      // loop around the one or more value-duration pairs  of this type:
      while (pos<len  && msg.charAt(pos) != delimProperty && msg.charAt(pos) != delimMeasure) {
        val = 0;
        durationVal = 0;
        durationSeconds = 0;
        while((pos<len) && ((int)msg.charAt(pos) <= (int)'9') && ((int)msg.charAt(pos)>=(int)'0')) {
          durationVal = durationVal * 10;
          durationVal = durationVal + (unsigned int)((unsigned int)msg.charAt(pos) - (unsigned int)'0');
          pos++;
        }
        // now get duration Type can be s m h or d seconds minuites hours or days:
        if (pos<len) {
          durationType = msg.charAt(pos++);
          switch (durationType) {
            case 's':
              durationSeconds = durationVal;
            break;
            case 'm':
              durationSeconds = 60L * durationVal;
            break;
            case 'h':
              durationSeconds = 3600L * durationVal;
            break;
            case 'd':
              durationSeconds = 86400L * durationVal;
            break;
            default:
              if (debug) Serial.printf("Invalid duration type '%c'. valUnits was %d\n",durationType,valUnits);
              durationSeconds = 0;
              valUnits = 0;// this makes us skip this measure - valUnits == 0 when units are not valid.
              // advance pos to the end of this meanure:
              while (pos<len  && msg.charAt(pos) != delimProperty && msg.charAt(pos) != delimMeasure) pos++;
          }
        }
        //find the value
        while((pos<len) && ((int)msg.charAt(pos) <= (int)'9') && (((int)msg.charAt(pos)>=(int)'0'))) {
          val = val * 10;
          val = val + (unsigned int)((unsigned int)msg.charAt(pos) - (unsigned int)'0');
          pos++;
        }
        if (measureType && valUnits) {
          // This is where we need to take action in response to duration - value pairs we care about.
          Serial.printf("End of value-duration pair: %c duration %d%c (%lu seconds) val %d\n",measureTypeChar,durationVal,durationType,durationSeconds,val);
  
          if (TYPE_MOTION == measureType) {
            if (MOTION_SENSOR_LIGHT_CONTROL) {
              // turn light on as soon as motion detected - so check for val 100 and duration 0:
              if ( 100 == val && 0 == durationSeconds && ( ! motionSensorRelayStatus)) {
                lastMotion = millis();
                motionSensorRelayStatus = true;
                Serial.println("Turning sensor light on");
                digitalWrite(MOTION_SENSOR_LIGHT_CONTROL,HIGH);
              } else {
                // if the sensor light is on, check if there has been reliable motion detected:
                if ( durationSeconds > 4 && val >=50 && motionSensorRelayStatus) {
                  lastMotion = millis();
                  if (debug) Serial.println("Motion detected while light on");
                } else {
                  
                }
              }
            } else {
              if (debug) Serial.println("Motion deteceted but this device not a motion sensor light");
            }
          } else if (TYPE_SET_HEATER_TARGET_TEMP == measureType) {
            // set thermostat set point
            heaterTargetdC = val;
            if (debug) Serial.printf("Heater set point set to %d/10 deg C\n",heaterTargetdC);
            //ToDo: Display the set point on a screen
          } else  if (TYPE_TEMP == measureType) {
            if (HEATER_CONTROL) {
              if (durationSeconds > 9) {
                if (debug) Serial.printf("Temp now %d (duration %d) thermostat set point %d",val,durationSeconds,heaterTargetdC);
                if (val > heaterTargetdC) {
                  if (heaterControlStatus) {
                    Serial.println(" so turning heater off");
                    digitalWrite(HEATER_CONTROL, LOW);
                    digitalWrite(LED,HIGH);
                    heaterControlStatus = false;
                  } else {
                    Serial.println(" and heater off already so doing nothing");
                  }
                } else {
                  if (heaterControlStatus) {
                    Serial.println(" and heater ON already so doing nothing");
                  } else {
                    Serial.println(" so turning heater on");
                    digitalWrite(HEATER_CONTROL, HIGH);
                    digitalWrite(LED,LOW);
                    heaterControlStatus = true;
                  }
                }
              } else {
                if (debug) Serial.println("This device is a heater controler but the temp message duration was less than 10 seconds so was ignored");
              }
            } else {
              if (debug) Serial.println("Device not a heater but did recieve the temp message.");
            }
          } else {
            if (debug) Serial.println("Could not find a valid measurement type and units");
          }
        }
        if (pos == len) {
          if (debug) Serial.printf("Entire message Parsed correctly.");
        } else {
          if (delimMeasure == msg.charAt(pos)) {
            // there is another message, so increment pos again and let the loop process it
            pos++;
            Serial.printf("found the mesure-duration pair delimiter %c\n",delimMeasure);
          }
        }
      }
      if (msg.charAt(pos) == delimProperty) pos++;
    } else {
      // units and/or measure type not valid so move pos foward to next measure pair:
      while (pos<len && msg.charAt(pos) != delimMeasure) pos++;
      if (debug) Serial.printf("Moved pos forward to %d\n",pos);
    }
    if (debug) Serial.printf("End of measurement type loop pos=%d\n",pos);
    
  }
  if (debug) Serial.println("Parse complete.");
  return retval;
}

void callback(char* topic, byte* payload, unsigned int length) {
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.printf("],%s\n",payload);
 
 parseMsg(String((char * )payload));
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nC:/Users/epico/Documents/ArduinoAndElectronics/ArduinoSketches/SonoffMqttSub\nrelay control %d\n",RELAY_CONTROL);
  setup_wifi();
  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(RELAY_CONTROL, OUTPUT);
  pinMode(LED,OUTPUT);
  Serial.println("Blinking relay and LED");
  digitalWrite(RELAY_CONTROL, LOW);
  digitalWrite(LED,HIGH);
  delay(500);
  digitalWrite(RELAY_CONTROL, HIGH);
  digitalWrite(LED,LOW);
  delay(500);
  digitalWrite(RELAY_CONTROL, LOW);
  digitalWrite(LED,HIGH);
  Serial.println("Blink of relay and LED flash done");
  
  motionSensorRelayStatus = LOW;
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP8266Client")){//, mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
  
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.subscribe(in_topic);
  if (MOTION_SENSOR_LIGHT_CONTROL && ((millis() - lastMotion) / 1000) > LEAVE_ON_FOR_SECONDS && true == motionSensorRelayStatus) {
    Serial.println("Turned off sensor light");
    motionSensorRelayStatus = false;
    digitalWrite(MOTION_SENSOR_LIGHT_CONTROL,LOW);
  }
  
}
