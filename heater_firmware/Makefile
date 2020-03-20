BOARD_FQN := esp8266:esp8266:nodemcuv2
SERIAL_PORT ?= /dev/ttyUSB0

TARGET := heater_firmware.bin

SRCS := board.h \
		commissioned.cpp commissioned.h \
		heater_firmware.ino \
		settings.cpp settings.h \
		uncomissioned.cpp uncommissioned.h \
		webpages.cpp webpages.h

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	arduino-cli compile -b $(BOARD_FQN) --build-path $(CURDIR)/build --build-cache-path $(CURDIR)/.cache -o $@

$(SRCS):

.PHONY: upload
upload: $(TARGET)
	arduino-cli upload -b $(BOARD_FQN) -p $(SERIAL_PORT) -i $^

.PHONY: term
term: upload
	picocom -b 115200 $(SERIAL_PORT)

.PHONY: distclean
distclean:
	rm -rf build .cache
	rm -f *.bin *.elf
