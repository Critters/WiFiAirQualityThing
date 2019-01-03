When the Camp Fire peaked in California I purchased a handheld air quality sensor and was amazed at how poor the air quality was both indoors and out. It convinced me to get an air filter for my home and identified an issue with the office air filtration. The sensor was loaned to a few friends and was passed around for a while, people found hot spots in their homes and it convinced others to wear masks

It was expensive, and though it had a nice display and measured a bunch of things it didn't store the data or let you extract it, all information was lossed when it was powered down

This project pairs an inexpensive sensor with an equally inexpensive NodeMCU (tiny Arduino board with WiFi) to make a module that will log air quality to Adafruit.IO so you can monitor air quality remotely and have a record. Total build cost is about $20

# FILES
- arduino.ino - the code to upload to the nodeMCU. Replace the strings at the top with your WiFi and Adafruit.IO details and upload using the standard Arduino IDE, google "NodeMCU Arduino" for instructions on adding the board to the IDE
- housing-inside.stl & housing-outside.stl - 3D printable housing

# BOM
- "PMS5003 High Precision Laser Dust Sensor Module" Can be found on eBay for about $16 each from China or double that if you are in a rush and want a US supplier
- "NodeMCU" can also be found on eBay for about $5

## OPTIONAL (if you print the enclosure)
- 5x M3x8 screws
- Zip Tie to fix sensor to housing, though it is a tight fit so not needed if you don't plan to knock it around

# WIRING
Looking at the back of the sensor where the pins are, if you have the pins on the right side of the unit, PIN1 is the left-most pin. See the included "img-pins.jpg" file
- PIN1 - Vin
- PIN2 - Ground
- PIN5 - D7

All other pins are unused. When powered you should hear the fan turn if you hold it to your ear (it is fairly quiet) and with the code uploaded you should see "Started" and data showing up in the ArduinoIDE console

# USAGE
Once you have wired it up and modified the code to include your WiFi and Adafruit.IO details you can just power it up and it will start logging data. If you make more than one just give each one a unique "GROUP_KEY". On your Adafruit.IO page you should see the data appear on your feed, you DON'T need to define the feed names before sending data, they are created automatically
