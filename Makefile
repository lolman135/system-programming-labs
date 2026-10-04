IMAGE ?= stm32-build
DOCKER_RUN = docker run --rm -v "$(CURDIR)":/workspace -w /workspace \
             --user $$(id -u):$$(id -g) $(IMAGE)

.PHONY: build clean image

image:
	docker build -t $(IMAGE) docker/

build:
	$(DOCKER_RUN) make -C firmware all

clean:
	$(DOCKER_RUN) make -C firmware clean