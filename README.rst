Welcome to the micro-mouse robots project
=========================================

.. topic:: Abstract

   On this page you will learn how we designed our robots.

   * How does the high level C code communicate with the micro-controller
     and other hardware components ?
   * How do we manage to bring the micro-mouses at the right speed and in the right direction
     at each step all along the way ?
   * How do we manage to detect obstacles ?

   Those are only a few examples of all the questions that will be addressed here :-)

.. contents::
   :local:

Overview
--------

motors
------

Part 1
^^^^^^

Part 2
^^^^^^

Bluetooth
---------
# download the driver for usb/serial adapter : CP210x USB to UART Bridge VCP Drivers
.. seealso::

	https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers

# usefull documentation 
.. seealso::
   
   https://www.dfrobot.com/wiki/index.php/DF-BluetoothV3_Bluetooth_module_(SKU:TEL0026)
   
# full AT commands
.. seealso::
   
   http://image.dfrobot.com/image/data/TEL0026/TEL0026_Datasheet.pdf

# How to configure the bluetooth module: 
# Default serial software configuration is : baudrate 38400 + echo local + line mode + AT swith ON (see bluetooth module swith)  
# Use hyper terminal or other serial console, set baud rate 38400, data 8 bit, stop bit 1, no parity, no flow control. 

# IMPORTANT : the baudrate in AT mode is ALWAYS 38400, even if you change the UART speed to other value than 38400.

.. code-block:: bash

   # Reset module to factory settings

   > AT+ORGL
   OK
   
   # Config robot side

   > AT
   OK
   
   > AT+PSWD?
   +PSWD:1234
 
   > AT+NAME=ROBOT_FRANCOIS
   OK
   
   > AT+UART=115200,0,0
   OK

   > AT+ROLE=0
   OK


.. code-block:: bash

   # Config PC side

   > AT
   OK
   
   > AT+PSWD?
   +PSWD:1234
 
   > AT+NAME=PC_FRANCOIS
   OK
   
   > AT+UART=115200,0,0
   OK

   > AT+ROLE=1
   OK

Gyroscope
---------

Infrared LEDS and photo-sensitive transistors
---------------------------------------------

Buzzer
------

Battery
-------
#HW: Robot is powered with a LIPO 2S (8.4V) 200mA.h (or more) battery, connected to the STM32 (PA3), through a voltage divider (ratio about 1:10).

#HAL: PA3 input is configured as Alternate Function (ADC3). ADC3 channel 3 (IN3) is configured to measure battery voltage, with 12 bits resolution, continuous scan conversion and continuous DMA requests, with a maximum conversion cycles (480 cycles) per measure. A DMA is configured to transfers ADC measures in circular mode with auto increment memory address.

#Software: ADC3 is started at Power On Reset in DMA mode (HAL_ADC_Start_DMA(&hadc3..)) using HAL API. User RAM is then periodically updated (overwrited) with last ADC3 channel 3 measure. A simple user function applies the ratio (once calibrated), in order to provide battery voltage in Volts (float) from ADC3 channel 3 measures (uint16). When battery voltage is lower than about 3.2V per element (6.4V), robot shall stops (FAILSAFE mode). Main robot FSM checks the battery voltage at the begining of each run. While running (learning run or fast run), the robot does'nt stop until end of run.

Build with Atollic
------------------
1. generate the code thanks to STM32CubeMX, drivers files and loader file _FLASH.ld are generated
2. before building the Atollic project, open the project->build settings, and add the following
   compiler options to the assembler and C compiler command : -mcpu=cortex-m3 -mthumb 
	- resulting assembler compiler command: arm-atollic-eabi-gcc -c -mcpu=cortex-m3 -mthumb
	- resulting C complier command:			arm-atollic-eabi-gcc -c -mcpu=cortex-m3 -mthumb 
3. make sure the floating point is set to software implementation in assembler->target->floating point and
c compiler->target->floating point
4. select C linker->general to set the linker script in the linker script (example: GitHub\micromouse\_FLASH.ld)
5. in the file Drivers/ST/STM32F7xx/Include/stm32f7xx.h, uncomment the define STM32F767xx.
6. you may build without errors 