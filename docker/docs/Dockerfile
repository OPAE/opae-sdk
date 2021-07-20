FROM ubuntu:latest
RUN apt update
RUN apt install apt-utils
RUN apt install -y tzdata && dpkg-reconfigure --frontend noninteractive tzdata
RUN apt install -y build-essential cmake git libjson-c-dev uuid-dev git python3-dev python3-wheel python3-pip doxygen libedit-dev libcap-dev
WORKDIR /root
ENTRYPOINT [ "/bin/bash", "-c" ]


