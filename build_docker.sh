#!/bin/bash

# make the image, you can comment this after
# docker build --platform linux/arm64 --build-arg UID=$(id -u) --build-arg GID=$(id -g) -t batocera-build-arm64 .

# build the project
docker run --rm --platform linux/arm64 -v "${PWD}:/build" batocera-build-arm64 cmake .
docker run --rm --platform linux/arm64 -v "${PWD}:/build" batocera-build-arm64 make -j6
