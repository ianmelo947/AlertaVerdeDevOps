FROM nginx:alpine

# 1. Copia os arquivos do site
COPY . /usr/share/nginx/html

# 2. Escreve a configuração do Proxy (Sem usar COPY de arquivo externo)
RUN printf 'server {\n\
    listen 80;\n\
    server_name localhost;\n\
\n\
    location / {\n\
        root /usr/share/nginx/html;\n\
        index index.html index.htm;\n\
        try_files $uri $uri/ /index.html;\n\
    }\n\
\n\
    location /api/ {\n\
        proxy_pass http://backend:8080;\n\
        proxy_set_header Host $host;\n\
        proxy_set_header X-Real-IP $remote_addr;\n\
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;\n\
    }\n\
}' > /etc/nginx/conf.d/default.conf

EXPOSE 80