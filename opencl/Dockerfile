# Use a imagem oficial do Ubuntu como base
FROM ubuntu:22.04

# Defina uma variável de ambiente para evitar prompts durante a instalação
ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependências necessárias usando apt-get
RUN apt-get update && apt-get install -y \
    build-essential cmake git wget curl pkg-config \
    ocl-icd-opencl-dev opencl-headers \
    libpthread-stubs0-dev \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Instalar o Node.js (versão LTS) e npm
RUN curl -fsSL https://deb.nodesource.com/setup_16.x | bash - && \
    apt-get install -y nodejs

# Instalar o http-server globalmente
RUN npm install -g http-server

RUN apt-get install -y clinfo

# Definir o diretório de trabalho
WORKDIR /usr/src/app

# Copiar os arquivos necessários para dentro do contêiner
COPY app /usr/src/app/app       
COPY style.css /usr/src/app/style.css
COPY index.html /usr/src/app/index.html
COPY json.hpp ./json.hpp  

# Expor a porta 8080
EXPOSE 8080

# Comando para rodar o app.js
CMD ["http-server", ".", "-p", "8080"]
