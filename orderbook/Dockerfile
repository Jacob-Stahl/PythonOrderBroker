FROM gcc:latest

# Install cmake
RUN apt-get update && apt-get install -y cmake

# Move to app directory and copy files
WORKDIR /app
COPY . .
RUN cmake . && make

CMD ["./orderbook"]