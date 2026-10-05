IMAGE       ?= stm32-build
WOKWI_CLI   ?= wokwi-cli
SIM_TIMEOUT ?= 600000

DOCKER_RUN = docker run --rm -v "$(CURDIR)":/workspace -w /workspace \
             --user $$(id -u):$$(id -g) $(IMAGE)

.DEFAULT_GOAL := build
.PHONY: build clean image flash monitor check-wokwi

image:
	docker build -t $(IMAGE) docker/

build:
	$(DOCKER_RUN) make -C firmware all

clean:
	$(DOCKER_RUN) make -C firmware clean

flash: build check-wokwi
	$(WOKWI_CLI) --timeout 5000 --expect-text "Bring-up checks" --fail-text "FAIL"

monitor: check-wokwi
	$(WOKWI_CLI) --interactive --timeout $(SIM_TIMEOUT) .

check-wokwi:
	@command -v $(WOKWI_CLI) >/dev/null || { echo "wokwi-cli не найден: curl -L https://wokwi.com/ci/install.sh | sh"; exit 1; }
	@test -n "$$WOKWI_CLI_TOKEN" || { echo "WOKWI_CLI_TOKEN не задан: https://wokwi.com/dashboard/ci"; exit 1; }