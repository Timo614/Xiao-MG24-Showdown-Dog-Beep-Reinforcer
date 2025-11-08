This project demonstrates a Xiao MG24 and a custom Xiao footprint board used for sending reinforcement beeps to dog collars that operate on the 433 MHz frequency. My dogs are heavy barkers and, given their nature as rescue dogs, are jittery when it comes to people walking by outside. We ended up getting these collars off Amazon as we had issues where they would run in the backyard and excessively bark at neighbors. I opted to only store the beep element as we don't use the other features of the collars and a single beep is enough to get them to stop in most cases.

## Program Capabilities

The project in its final form has the Xiao MG24 board connected via headers to my Explorer 433 expansion board. The expansion board has a light sensor, AHT21, RFM69HCW, and two HY2.0 "Grove" inputs (one for i2c and another for uart). 

I've provided two ino files for Arduino:
- src/Explorer433-Matter.ino - a project which utilizes the Matter over Thread functionality of the Xiao MG24 to send temperature information from the AHT21 and allow triggering of dogs via Matter
- src/Explorer433.ino - a project utilizing an I2C based Grove OLED Display and a Grove button connected to the RX pin of the Xiao MG24 to demonstrate functionality in an off grid manner

The Xiao MG24 is connected to the RFM69HCW via the Xiao's pins with SPI setup, a CS and Reset pin connected, and two pins connected for DIO0 and DIO2 (which is used for the transmit mode I am using here). 

I have two demonstration videos which correspond with each of these projects. The latter video was from before the Xiao Showdown extension while the former is from after. I was able to setup a Thread Border Router on my Home Assistant over the past week and utilize the Matter functionality allowing me to take advantage of the onboard Matter over Thread capabilities.

The first demonstration video showcases the device, it being used with home assistant, and then dives into the creation process with some additional demonstrations such as the steps to record on the Flipper device for gathering the initial data.

The second demonstration video shows the AHT21 being polled and updating its status via the OLED display along with the button being used for triggering a specific collar based on a number of presses in a set window. The Xiao footprint is nice as it is small and given multiple microcontrollers from Seeed use it you can build a board and swap out the Xiao based on your needs (Thread? MG24, WIFI? ESP32C3, Bluetooth nrf52840).

## Getting Dog Collar Signals

To get the dog collar signals I used here I used my Flipper Zero device set to record on the frequency scanner and further saving them. I then took out the SD card and extracted the sub files for each collar that contained the read messages. I then utilized a script I put together to parse the sub file and export the timing as a header file I could include in my project. These header files are what were used here for the project to trigger the collars.

## Structure

- README: This file
- DogBeepFlipperSubJupyterCell.py: Jupyter cell for converting sub file to format needed
- src: Folder containing end project Arduino logic
- Hardware: Folder containing Schematic, BOM, and Gerber for the Explorer 433 board
- Enclosure: Folder containing STL files for printing the enclosure for the hardware

## Dependencies

To properly use the project several dependencies are needed on the Arduino side:
- Adafruit AHTX0
- U8g2 (only used by the second project)

Please install these libraries and their dependencies to properly compile the Arduino code.