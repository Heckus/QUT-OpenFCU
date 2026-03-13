# Use
All tests were done prior to PX4 firmware being flashed. These were done using STM32 tools (IDE,MX,Programmer)

The project config has been saved and allsensor.c is what needs to be placed inside. Allsensor.c tests spi connections to validate physical solder joints, flashing appropriate error lights. Its key use is when this board was being hand soldered and joints needed to be verified

Core/Src/main.c is where allsensor.c must go

To edit pinouts use MX software

to flash code compile with STM IDE and use STM Programmer to flash elf via dfu