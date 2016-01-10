ARDUINO_DIR       = $(realpath $(HOME)/arduino/arduino-current)
ARDUINO_LIBS      := SPI Wire \
	Adafruit_GFX Adafruit_SSD1306 \
	Adafruit_SI1145

ARDUINO_PORT      := /dev/ttyACM0
USER_LIB_PATH     := $(realpath ./lib)
ARCHITECTURE      = avr
BOARD_TAG         = leonardo

#CXXFLAGS      += -std=gnu++11 -Wl,-u,vfprintf
#CFLAGS        += -std=gnu11 -Wl,-u,vfprintf
#LDFLAGS       += -lprintf_flt -lm -Wl,-u,vfprintf
CXXFLAGS      += -std=gnu++11
CFLAGS        += -std=gnu11

include Arduino.mk
