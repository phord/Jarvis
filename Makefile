BUILD_DIR = ../build
BOARD = esp8266:esp8266:oak

# Serial uploads
UPLOAD_PORT = /dev/ttyUSB1
TARGET_FLAGS += -b $(BOARD)

ESPOTA = $(HOME)/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py

COMPILE_FLAGS = --build-cache-path $(BUILD_DIR) --output-dir $(BUILD_DIR)
COMPILE_FLAGS += $(TARGET_FLAGS) --warnings default

.PHONY: all build upload
all: upload

build:
	arduino-cli compile $(COMPILE_FLAGS) .

upload: build
	$(ESPOTA) -i jarvis.local -p 8266 -f $(BUILD_DIR)/Jarvis.ino.bin

upload-serial: build
	arduino-cli upload --port $(UPLOAD_PORT) $(TARGET_FLAGS) .
