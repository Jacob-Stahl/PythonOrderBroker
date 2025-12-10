FROM gcc:latest

# Install cmake
RUN apt-get update && apt-get install -y cmake

# Install and build google test
RUN apt-get update && apt-get install -y libgtest-dev
RUN cd /usr/src/gtest && cmake . && make && cp lib/*.a /usr/lib

# Build and test the orderbook service
WORKDIR /app
COPY . .
RUN cmake . && make
RUN ctest --output-on-failure

# Run the orderbook service
CMD ["./orderbook"]