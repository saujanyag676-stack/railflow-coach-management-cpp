FROM gcc:14 AS build
WORKDIR /src
COPY main.cpp .
RUN g++ -std=c++17 -O2 -pthread main.cpp -o railflow

FROM debian:bookworm-slim
COPY --from=build /src/railflow /usr/local/bin/railflow
ENV PORT=8000
EXPOSE 8000
CMD ["railflow"]
