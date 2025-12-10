FROM gcc:latest
WORKDIR /app
COPY . .
RUN g++ -o orderbook main.cpp
CMD ["./orderbook"]