# https://github.com/arduino/arduino-cli/releases

port := $(shell python3 board_detect.py)

default:
	arduino-cli compile --fqbn=esp8266:esp8266:d1_mini_clone server -e

# Define the target 'upload'
upload:
	@while true; do \
		port=$$(python3 board_detect.py); \
		if [ -z "$$port" ]; then \
			echo "\n\n\t  ESP8266 PORT NOT FOUND"; \
			echo "Plug it in    OR    Press CTL/C to exit"; \
			sleep 10; \
		else \
			echo "Port detected: $$port"; \
			echo "Uploading..."; \
			arduino-cli upload -p "$$port" --fqbn esp8266:esp8266:d1_mini_clone server; \
			echo "Upload completed successfully."; \
			exit 0; \
		fi; \
	done

install_platform:
	arduino-cli config init --overwrite
	arduino-cli core update-index --config-file arduino-cli.yaml
	arduino-cli core install esp8266:esp8266 --config-file arduino-cli.yaml

deps:
	arduino-cli lib install "TinyGPSPlus"
	arduino-cli lib install "EspSoftwareSerial"

install_arduino_cli:
	mkdir -p ~/.local/bin
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh

	@echo "\n!!! Please add ~/.local/bin to PATH !!!"
