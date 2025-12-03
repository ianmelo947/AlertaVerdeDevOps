# Backend + OpenCL Worker + Docker Compose

## Conteúdo
- frontend files: index.html, app.js, style.css (copied from your upload or placeholders)
- backend/: Node.js backend (Express + SQLite + JWT)
- opencl/: C++ OpenCL worker (worker.cpp) + Dockerfile. You must download two single-file headers:
  - httplib.h (cpp-httplib)
  - json.hpp (nlohmann json)

## Passos rápidos
1. Coloque `httplib.h` e `json.hpp` dentro da pasta `opencl/` (links in opencl/README_HEADERS.md).
2. Build e start:
   ```
   docker compose build
   docker compose up -d
   ```
3. Backend servirá o frontend: http://localhost:8080
4. Teste compute endpoint (substitua <TOKEN>):
   ```
   curl -X POST http://localhost:8080/api/compute \
     -H "Authorization: Bearer <TOKEN>" \
     -H "Content-Type: application/json" \
     -d '{"op":"vec_add","size":1000000}'
   ```

## Notas
- Para usar GPU com drivers (NVIDIA), adapte o docker-compose para `--gpus all` e ajuste a imagem do worker.
- Troque `JWT_SECRET` por um segredo forte em produção.
