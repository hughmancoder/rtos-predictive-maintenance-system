# Top-level commands for both firmwares. Run `make` or `make help` to list them.
#
# Optional variables:
#   PORT=/dev/cu.usbserial-XXXX   serial port for ESP32 flash/monitor and the STM32 log
#   N=100                         number of clips per class for `make data`

PIO     ?= pio
PYTHON  ?= python3
STM32   := stm32
ESP32   := esp32
STM32_ENV := blackpill_f401cc
STM32_ELF := $(STM32)/.pio/build/$(STM32_ENV)/firmware.elf
READELF := $(HOME)/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-readelf
N       ?= 100

UPLOAD_PORT  := $(if $(PORT),--upload-port $(PORT))
MONITOR_PORT := $(if $(PORT),--port $(PORT))

.DEFAULT_GOAL := help
.PHONY: help all clean \
        stm32 stm32-flash stm32-monitor stm32-debug stm32-size stm32-fpcheck stm32-clean \
        esp32 esp32-flash esp32-monitor esp32-flash-monitor esp32-menuconfig esp32-size esp32-clean \
        data

help: ## List the available commands
	@awk 'BEGIN {FS = ":.*## "} /^[a-zA-Z0-9_-]+:.*## / {printf "  \033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

all: stm32 esp32 ## Build both firmwares

clean: stm32-clean esp32-clean ## Clean both builds

# --- STM32 (Black Pill) ------------------------------------------------------
# platformio.ini loads stm32/hardfp.py as a pre-build script, which adds the
# hard-float flags (-mfpu=fpv4-sp-d16 -mfloat-abi=hard) to every build below.

stm32: ## Build the STM32 firmware (hard-float, via hardfp.py)
	cd $(STM32) && $(PIO) run

stm32-flash: ## Build and flash the STM32 over ST-LINK
	cd $(STM32) && $(PIO) run -t upload

stm32-monitor: ## Open the STM32 debug log (USART2 via USB-UART)
	cd $(STM32) && $(PIO) device monitor $(MONITOR_PORT)

stm32-debug: ## Start a GDB session over ST-LINK
	cd $(STM32) && $(PIO) debug --interface=gdb -- -x .pioinit

stm32-size: stm32 ## Show STM32 flash/RAM use by section
	cd $(STM32) && $(PIO) run -t size

stm32-fpcheck: stm32 ## Check the STM32 ELF uses the hard-float ABI
	@$(READELF) -A $(STM32_ELF) | grep -E "Tag_FP_arch|Tag_ABI_VFP_args"
	@$(READELF) -A $(STM32_ELF) | grep -q "Tag_ABI_VFP_args: VFP registers" \
		&& echo "OK: hard-float ABI" || (echo "FAIL: not hard-float"; exit 1)

stm32-clean: ## Clean the STM32 build
	cd $(STM32) && $(PIO) run -t clean

# --- ESP32-S3 ----------------------------------------------------------------

esp32: ## Build the ESP32-S3 firmware
	cd $(ESP32) && $(PIO) run

esp32-flash: ## Build and flash the ESP32-S3 (use the UART USB port)
	cd $(ESP32) && $(PIO) run -t upload $(UPLOAD_PORT)

esp32-monitor: ## Open the ESP32-S3 serial monitor
	cd $(ESP32) && $(PIO) device monitor $(MONITOR_PORT)

esp32-flash-monitor: ## Flash the ESP32-S3, then open the monitor
	cd $(ESP32) && $(PIO) run -t upload -t monitor $(UPLOAD_PORT) $(if $(PORT),--monitor-port $(PORT))

esp32-menuconfig: ## Open ESP-IDF menuconfig
	cd $(ESP32) && $(PIO) run -t menuconfig

esp32-size: esp32 ## Show ESP32-S3 flash/RAM use
	cd $(ESP32) && $(PIO) run -t size

esp32-clean: ## Clean the ESP32-S3 build
	cd $(ESP32) && $(PIO) run -t clean

# --- Data --------------------------------------------------------------------

data: ## Download the MIMII fan subset into data/ (N clips per class)
	$(PYTHON) -I tools/fetch_mimii.py $(N)
