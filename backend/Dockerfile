FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Instalar compilador, sqlite, openssl E AGORA AS LIBS OPENCL (ocl-icd, opencl-headers, pocl)
RUN apt-get update && apt-get install -y \
    g++ \
    libsqlite3-dev \
    libssl-dev \
    ocl-icd-opencl-dev \
    opencl-headers \
    pocl-opencl-icd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

# Copiar código fonte ( ͡° ͜ʖ ͡°)
COPY main.cpp .

# Compilar (Adicionamos a flag -lOpenCL)
RUN g++ -std=c++11 -o backend main.cpp -lsqlite3 -lssl -lcrypto -lOpenCL -lpthread

# Criar diretório de dados :)
RUN mkdir -p /data

EXPOSE 8080

CMD ["./backend"]