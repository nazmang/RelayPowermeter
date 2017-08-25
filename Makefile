# Arduino Make file. Refer to https://github.com/sudar/Arduino-Makefile

BOARD_TAG    = uno
ISP_PROG     = usbasp
AVRDUDE_OPTS = -v
ARDUINO_SKETCHBOOK = ../../Arduino
ARDUINO_LIBS = Time SPI DS3232RTC MySensors Bounce2 Wire
include ../Arduino-Makefile/Arduino.mk
