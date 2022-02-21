#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := Brexit
SUFFIX := $(shell components/ESP32-RevK/suffix)

all:
	idf.py build
	cp build/$(PROJECT_NAME).bin build/$(PROJECT_NAME)$(SUFFIX).bin

flash:
	idf.py flash

monitor:
	idf.py monitor

clean:
	idf.py clean

menuconfig:
	idf.py menuconfig

pull:
	git pull
	git submodule update --recursive

update:
	git submodule update --init --recursive --remote
	git commit -a -m "Library update"
