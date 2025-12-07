FROM nginx:alpine

# Copia os arquivos do site
COPY . /usr/share/nginx/html

# Copia a configuração personalizada do Nginx
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 80