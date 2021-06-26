FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --assume-yes build-essential clang cmake git \
  && update-alternatives --set c++ /usr/bin/clang++

CMD ["bash"]