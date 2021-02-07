BOARD_FQN := esp8266:esp8266:nodemcuv2
SERIAL_PORT ?= /dev/ttyUSB0

BINDIR := bin

TARGET := heater_firmware.bin

SRCS := board.h \
		commissioned.cpp commissioned.h \
		heater.h \
		heater_firmware.ino \
		settings.cpp settings.h \
		uncommissioned.cpp uncommissioned.h \
		version.h \
		webpages.cpp webpages.h

GIT_HASH := $(shell git rev-parse HEAD | head -c 12)$(shell git diff --quiet || echo '-dirty')
BUILD_TIME := $(shell date +%F-%T)
CFLAGS += -DGIT_HASH=$(GIT_HASH) -DBUILD_TIME=$(BUILD_TIME)

.PHONY: all
all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(SRCS) | $(BINDIR)
	arduino-cli compile -b $(BOARD_FQN) --build-properties compiler.cpp.extra_flags="$(CFLAGS)" --build-path $(CURDIR)/build --build-cache-path $(CURDIR)/.cache -o $@

$(SRCS):

$(BINDIR):
	mkdir -p $@

.PHONY: upload
upload: $(BINDIR)/$(TARGET)
	arduino-cli upload -b $(BOARD_FQN) -p $(SERIAL_PORT) -i $^

.PHONY: term
term: upload
	picocom -b 115200 $(SERIAL_PORT)

.PHONY: distclean
distclean:
	rm -rf build .cache $(BINDIR)
	rm -f *.bin *.elf
