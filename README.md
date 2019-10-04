# esp8266-mqtt-tx
Arduino Project enabling an ESP8266 to send regular MQTT updates about GPIO input values to a mesh of Epicom IOT Devies.
This project implements the Epicom IOT Protocol over MQTT, allowing ESP8266 devices to join an Epicom IOT Mesh. This code allows other Epicom IOT Mesh devices to react to the temperature or motion detection by ESP8266 sensors within the mesh.
# Message Format
## Message Type
The first character indicates the message type, one of:
* ```M``` Measurment - detected physical values broadcast by a sensor
* ```C``` Calibration - Tells temperature sensors what the current actual temperature is allowing them to calibrate
* ```D``` Debug message - human readable system messages indicating what is going on
* ```S``` Status message - sent by devices when they turn things on or off, allowing remote displays to display the new status
* ```P``` Parameter Update - allows changes to thermostat set points or other parameters.

This code sends `M` (Measurement) type messages.
## 'Measurement' Type Message Format
Measurment messages are more than just a "The value is..." message. They are "The average value over the last 5 seconds was...". The measurement messages sent over MQTT have the following structure:
```MTdC:0s163:5s163:30s163:300s162:```
The colon delimits the message into one header section and one or more payload sections. Note that the trailing colon is advised as some MQTT implementation append trailing characters to the transmitted message. The trailing characters can contain the transmitting devices name or other characters. By having a trailing colon, we ensure that the last payload component's value is not corrupted by this trailing suffix. See below for more information on how the trailing suffix is handled by the message parser.
### Header Format
The measurement message header is made up of:
* 'M' first character indicates message is a measurment message
* Character indicating what property is being meaured.
  * Current measurment types:
    * `T` Temperature
    * `M` Motion
  * Future Measurement Types:
    * `L` Light Level
    * `N` Noise Level
* Units of the payload value. One of:
  * dC - demi degrees Celcius ( 211 = 21.1 degrees celcius, 395 = 39.5 degrees celcius
  * % - percentage - for Motion, what percentage of the measurement duration was the motion sensor detecting motion.
### Measurement Payload Format
One or more payload sections follow the header, delimited by a colon. Each payload section is made up of a duration and a measurment.
The payload sections are two decimal numbers separated by a duration units character
### Measurement Payload Duration
One or more decimal digits (0 to 9) followed by a duratation units character. The duration 0 indicates an instantaneous measurement - which is the *last* measurement taken by the sensor. Zero durations must also have a duration units character (eg 0s for zero seconds) to act as a delimiter between the zero duration and the value.
### Measrement Value
One or more decimal digits (0 to 9) following the duration units character, terminated by the measurement payload section delimiting colon. Values are positive integers from zero to 256.
### Example Payload Sections
`0s163` indicates a sensors last reading was 163. The units of the measurement are taken frm the header packet for this message.
`5s163` the average ove the last 5 seconds was 163
`30s163` the average over the last 30 seconds was 163.
`300s162` the average over the last 5 minutes (300 seconds) was 162.
### Valid Duration Unit Characters
* `s` seconds
* `m` minutes
* `h` hours
* `d` days
# Motion Detected Percentages
## Motion Sensor False Positive Motion Detection Consequences
Motion sensors in the mesh can trigger various effects. "Sensor Light" motion sensors can trigger lights to turn on for a short time, and "Someone is home" motion sensors can trigger home heaters to come on for several hours.
Motion sensors also regularly trigger spuriously when no humans are moving neat them. Outdoor motion sensors can be triggered by moths (an event made more common by infrared "night vision" cameras attracting them). Indoor sensors may be triggered by a spider crawling across the sensor, or by pets moving through a room.
Specifically in the case of a motion sensor triggering a heater to come on, the cost of a false positive can be high. A house can be put into "No one is home" mode when no motion is detected for 12 hours, meaning a heater scheduled to come on at 6am does not come on the next day, reducing the household heating costs. An office which has motion detected when it is closed for a weekend could trigger a heater to stay on for a whole weekend.
To combat this effect, the percentage of time for which motion is detected over various time periods is measured and broadcast.
For a sensor light, false positive motions detections may be tolerated. Sensor lights can turns on the moment motion is detected, as rapid response is desirable, and the cost of having a light on for 30 seconds is low.
For a heater control, we require that motion is detected for a percentage of the time (for example 20%) over a longer period of (for example) 2 minutes. This avoids the expense of heating an empty building, and because the response of a the actual temperature of a house to a heater coming on is delayed, the additional delay of 2 minutes before a heater turns on is a justified trade off against the savings from avoiding pointless heating triggered by spurious motion detection.

