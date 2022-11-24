# [Choice] Python version (use -bullseye variants on local arm64/Apple Silicon): 3, 3.10, 3.9, 3.8, 3.7, 3.6, 3-bullseye, 3.10-bullseye, 3.9-bullseye, 3.8-bullseye, 3.7-bullseye, 3.6-bullseye, 3-buster, 3.10-buster, 3.9-buster, 3.8-buster, 3.7-buster, 3.6-buster
ARG VARIANT=stretch
ARG TAG=v2.8.2
FROM mcr.microsoft.com/vscode/devcontainers/base:${VARIANT}

ENV DEBIAN_FRONTEND noninteractive

# Ensure that we always use UTF-8, US English locale and UTC time
RUN apt-get update && apt-get install -y locales && \
  localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8 && \
  echo "UTC" > /etc/timezone && \
  chmod 0755 /etc/timezone 
ENV LANG en_US.utf8
ENV LC_ALL=en_US.utf-8
ENV LANGUAGE=en_US:en
ENV PYTHONIOENCODING=utf-8

# Install the required packages to build LinuxCNC
RUN apt-get install -y \
      git \
      build-essential \
      pkg-config \
      curl \
      autogen \
      autoconf \
      python \
      python-tk \
      libffi-dev \
      libudev-dev \
      libmodbus-dev \
      libusb-1.0-0-dev \
      libgtk2.0-dev \
      python-gtk2 \
      procps \
      kmod \
      intltool \
      tcl8.6-dev \
      tk8.6-dev \
      bwidget \
      libtk-img \
      tclx \
      libreadline-gplv2-dev \
      libboost-python-dev \
      libglu1-mesa-dev \
      libgl1-mesa-dev \
      libxmu-dev \
      yapps2 \
      libjson-c-dev \
      libgdbm-dev \
      libncurses5-dev \
      libnspr4 \
      libnspr4-dev \
      libnss3 \
      libnss3-dev \
      libssl-dev 

WORKDIR /opt

# Install Python
RUN curl -O https://www.python.org/ftp/python/3.8.2/Python-3.8.2.tar.xz && \
    tar -xf Python-3.8.2.tar.xz && \ 
    cd Python-3.8.2 && \
    ./configure --enable-optimizations && \
    make -j 4 && \
    make install && \
    cd .. && \
    rm -r Python-3.8.2 && \
    rm Python-3.8.2.tar.xz

# Clone LinuxCNC, checkout the correct tag and build it
RUN git clone https://github.com/LinuxCNC/linuxcnc.git && \
    cd linuxcnc && \
    git checkout tags/v2.8.2 && \
    cd /opt/linuxcnc/debian && \
    ./configure uspace && \
    cd ../src && \
    ./autogen.sh && \
    ./configure --with-realtime=uspace &&\
    make && \
    echo '# set PATH so it includes linuxcnc binaries' >> /etc/profile && \
    echo 'if [ -d "/opt/linuxcnc/bin" ] ; then' >> /etc/profile && \
    echo 'PATH="/opt/linuxcnc/bin:$PATH"' >> /etc/profile && \
    echo 'fi' >> /etc/profile && \
    echo '# set PATH so it includes linuxcnc scripts' >> /etc/profile && \
    echo 'if [ -d "/opt/linuxcnc/scripts" ] ; then' >> /etc/profile && \
    echo 'PATH="/opt/linuxcnc/scripts:$PATH"' >> /etc/profile && \
    echo 'fi' >> /etc/profile
