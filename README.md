# Alerta Verde - Agricultura de Precisão (DevOps Edition)

Plataforma de monitoramento agrícola e simulação de colheita utilizando **Computação de Alta Performance (HPC)** e arquitetura de microsserviços em nuvem.

## Arquitetura do Projeto

O sistema é totalmente containerizado utilizando **Docker** e orquestrado via **Docker Compose V2**.

* **Frontend:** Nginx (Alpine) servindo uma SPA (Single Page Application) com Proxy Reverso.
* **Backend:** C++ 11 (Bare Metal) com biblioteca `httplib` e banco de dados SQLite embutido.
* **HPC (Simulação):** OpenCL (via POCL) para processamento paralelo de dados de colheita no Backend.
* **Observabilidade:** Stack completa de monitoramento com Zabbix Server, MySQL e Grafana.
* **CI/CD:** Pipeline automatizado via GitHub Actions com deploy na AWS EC2.

## Como Rodar (Instalação)

### Pré-requisitos
* Docker & Docker Compose V2
* Git

### Passos Rápidos
1.  Clone o repositório:
    ```bash
    git clone [https://github.com/ianmelo947/AlertaVerdeDevOps.git](https://github.com/ianmelo947/AlertaVerdeDevOps.git)
    cd AlertaVerdeDevOps
    ```

2.  Suba o ambiente (o build do C++ pode levar alguns minutos):
    ```bash
    docker compose up -d --build
    ```

3.  Acesse os serviços:
    * **Aplicação Web:** [http://localhost](http://localhost) (ou IP da AWS)
    * **API Backend:** Acessível via rota interna `/api`
    * **Grafana (Dashboards):** [http://localhost:3000](http://localhost:3000)
    * **Zabbix (Monitoramento):** [http://localhost:8081](http://localhost:8081)

## Credenciais de Acesso (Monitoramento)

| Serviço | URL | Usuário | Senha |
| :--- | :--- | :--- | :--- |
| **Grafana** | Porta 3000 | `admin` | `admin` (pedirá para trocar no 1º acesso) |
| **Zabbix** | Porta 8081 | `Admin` (Maiúsculo) | `zabbix` |
| **Aplicação** | Porta 80 | *Criar conta na tela* | *Sua senha* |

## Testando a API (Postman/cURL)

Para testar o motor de simulação OpenCL sem usar o navegador:

```bash
curl -X POST http://localhost/api/simulation \
  -H "Content-Type: application/json" \
  -d '{"area": "1000", "temp": "28.5"}'
