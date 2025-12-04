FROM arm64v8/debian:bookworm

RUN apt-get update -y && apt-get upgrade -y
RUN apt-get install ca-certificates openssl -y && update-ca-certificates
RUN apt-get install build-essential git curl -y

# batocera-emulationstation dependencies
RUN apt-get install libsdl2-dev libsdl2-mixer-dev libfreeimage-dev libfreetype6-dev \
  libcurl4-openssl-dev rapidjson-dev libasound2-dev libgl1-mesa-dev build-essential \
  libboost-all-dev cmake fonts-droid-fallback libvlc-dev libvlccore-dev vlc-bin libint-dev gettext -y

ARG UID=1000
ARG GID=1000
RUN groupadd -f -g $GID build && useradd -m -u $UID -g $GID build
USER build

WORKDIR /build
RUN git config --global --add safe.directory /build
