.PHONY: help build flash monitor menuconfig prod pbuild pmonitor pmenuconfig devbuild prodbuild devflash prodflash devmonitor prodmonitor devmenuconfig prodmenuconfig

PORT ?= /dev/ttyACM0
PROFILE ?= dev
PROFILE_SCRIPT := ./idf_profile.sh

help:
	@printf '%s\n' \
	  'Available targets:' \
	  '  make build                    Build development profile' \
	  '  make flash [PORT=...]         Flash development profile' \
	  '  make monitor [PORT=...]       Monitor development profile' \
	  '  make menuconfig              Edit development profile config' \
	  '  make prod [PORT=...]          Flash production profile' \
	  '  make pbuild                  Build production profile' \
	  '  make pmonitor [PORT=...]      Monitor production profile' \
	  '  make pmenuconfig             Edit production profile config' \
	  '  make devbuild                 Build development profile' \
	  '  make prodbuild                Build production profile' \
	  '  make devflash [PORT=...]      Flash development profile' \
	  '  make prodflash [PORT=...]     Flash production profile' \
	  '  make devmonitor [PORT=...]    Monitor development profile' \
	  '  make prodmonitor [PORT=...]   Monitor production profile' \
	  '  make devmenuconfig            Edit development profile config' \
	  '  make prodmenuconfig           Edit production profile config'


build: devbuild

flash: devflash

monitor: devmonitor

menuconfig: devmenuconfig

prod: prodflash

pbuild: prodbuild

pmonitor: prodmonitor

pmenuconfig: prodmenuconfig

devbuild:
	$(PROFILE_SCRIPT) dev build

prodbuild:
	$(PROFILE_SCRIPT) prod build

devflash:
	$(PROFILE_SCRIPT) dev -p $(PORT) flash

prodflash:
	$(PROFILE_SCRIPT) prod -p $(PORT) flash

devmonitor:
	$(PROFILE_SCRIPT) dev -p $(PORT) monitor

prodmonitor:
	$(PROFILE_SCRIPT) prod -p $(PORT) monitor

devmenuconfig:
	$(PROFILE_SCRIPT) dev menuconfig

prodmenuconfig:
	$(PROFILE_SCRIPT) prod menuconfig
