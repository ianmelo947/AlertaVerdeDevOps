
#ifndef CPP_HTTPLIB_LITE_H
#define CPP_HTTPLIB_LITE_H
#include <string>
#include <functional>
#include <thread>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

namespace httplib {

struct Request { std::string body; };

struct Response {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";
    void set_content(const std::string &b,const std::string &ct){
        body=b; content_type=ct;
    }
};

class Server {
public:
    std::function<void(const Request&, Response&)> post_handler;
    void Post(const std::string&, std::function<void(const Request&, Response&)> handler){
        post_handler = handler;
    }

    void listen(const char* ip, int port){
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(sockfd, (sockaddr*)&addr, sizeof(addr));
        ::listen(sockfd, 10);

        std::cout << "[httplib-lite] Servidor rodando em " << ip << ":" << port << std::endl;

        while(true){
            int client = accept(sockfd, nullptr, nullptr);

            char buffer[8192] = {0};
            read(client, buffer, sizeof(buffer));

            std::string raw(buffer);

            auto pos = raw.find("\r\n\r\n");
            std::string body = (pos != std::string::npos ? raw.substr(pos + 4) : "");

            Request req; req.body = body;
            Response res;

            if (post_handler) post_handler(req, res);

            std::stringstream ss;
            ss << "HTTP/1.1 " << res.status << " OK\r\n";
            ss << "Content-Type: " << res.content_type << "\r\n";
            ss << "Content-Length: " << res.body.size() << "\r\n\r\n";
            ss << res.body;

            auto out = ss.str();
            write(client, out.c_str(), out.size());
            close(client);
        }
    }
};

} // namespace httplib

#endif
