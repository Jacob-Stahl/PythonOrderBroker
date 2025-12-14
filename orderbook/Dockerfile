FROM gcc:latest

# Install cmake
RUN apt-get update && apt-get install -y cmake

# Paho MQTT client installation
# https://github.com/eclipse-paho/paho.mqtt.cpp
# Build tools + TLS deps
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build git pkg-config \
    libssl-dev ca-certificates \
 && rm -rf /var/lib/apt/lists/*

 # Build + install Paho C (required by Paho C++)
RUN git clone --depth=1 https://github.com/eclipse-paho/paho.mqtt.c.git /tmp/paho.mqtt.c \
 && cmake -S /tmp/paho.mqtt.c -B /tmp/paho.mqtt.c/build -G Ninja \
      -DPAHO_WITH_SSL=ON \
      -DPAHO_BUILD_SHARED=ON \
      -DPAHO_BUILD_STATIC=OFF \
      -DPAHO_ENABLE_TESTING=OFF \
 && cmake --build /tmp/paho.mqtt.c/build \
 && cmake --install /tmp/paho.mqtt.c/build \
 && rm -rf /tmp/paho.mqtt.c

# Build + install Paho C++
RUN git clone --depth=1 https://github.com/eclipse-paho/paho.mqtt.cpp.git /tmp/paho.mqtt.cpp \
 && cmake -S /tmp/paho.mqtt.cpp -B /tmp/paho.mqtt.cpp/build -G Ninja \
      -DPAHO_BUILD_SHARED=ON \
      -DPAHO_BUILD_STATIC=OFF \
      -DPAHO_BUILD_SAMPLES=OFF \
      -DPAHO_BUILD_TESTS=OFF \
 && cmake --build /tmp/paho.mqtt.cpp/build \
 && cmake --install /tmp/paho.mqtt.cpp/build \
 && rm -rf /tmp/paho.mqtt.cpp

# Move to app directory and copy files
WORKDIR /app
COPY . .
RUN cmake . && make

CMD ["./orderbook"]