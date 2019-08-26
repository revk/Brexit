#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := RevKCO2

include $(IDF_PATH)/make/project.mk

update:
	git submodule update --remote --merge
	git commit -a -m "Library update"
