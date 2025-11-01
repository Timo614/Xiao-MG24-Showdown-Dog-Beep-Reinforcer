This project demonstrates a Xiao MG24 and a custom Xiao footprint board used for sending reinforcement beeps to dog collars that operate on the 433 MHz frequency. My dogs are heavy barkers and, given their nature as rescue dogs, are jittery when it comes to people walking by outside. We ended up getting these collars off Amazon as we had issues where they would run in the backyard and excessively bark at neighbors. I opted to only store the beep element as we don't use the other features of the collars and a single beep is enough to get them to stop in most cases.

## Program Capabilities

The project in its final form has the Xiao MG24 board connected via headers to my Explorer 433 expansion board. The expansion board has a light sensor, AHT21, RFM69HCW, and two HY2.0 "Grove" inputs (one for i2c and another for uart). For the demonstration video I used an I2C based Grove OLED Display and a Grove Button connected to the RX pin of the Xiao MG24. The Xiao MG24 is connected to the RFM69HCW via the Xiao's pins with SPI setup, a CS and Reset pin connected, and two pins connected for DIO0 and DIO2 (which is used for the transmit mode I am using here). The demonstration video shows the AHT21 being polled and updating its status via the OLED display along with the button being used for triggering a specific collar based on a number of presses in a set window. The Xiao footprint is nice as it is small and given multiple microcontrollers from Seeed use it you can build a board and swap out the Xiao based on your needs (Thread? MG24, WIFI? ESP32C3, Bluetooth nrf52840).

## Getting Dog Collar Signals

To get the dog collar signals I used here I used my Flipper Zero device set to record on the frequency scanner and further saving them. I then took out the SD card and extracted the sub files for each collar that contained the read messages. I then utilized a script I put together to parse the sub file and export the timing as a header file I could include in my project. These header files are what were used here for the project to trigger the collars.

## Structure

- README: This file
- DogBeepFlipperSubJupyterCell.py: Jupyter cell for converting sub file to format needed
- src: Folder containing end project Arduino logic
