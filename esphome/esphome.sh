#!/bin/bash

docker run --rm --privileged -v "${PWD}":/config --device=/dev/ttyUSB0 --network host -it ghcr.io/esphome/esphome $@
