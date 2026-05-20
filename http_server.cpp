#include "http_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr int kBacklog = 16;
constexpr std::size_t kReadBufferSize = 8192;

std::runtime_error make_system_error(const std::string& what) {
    return std::runtime_error(what + ": " + std::strerror(errno));
}
}  // namespace

HttpServer::HttpServer(int port) : port_(port) {}

void HttpServer::run() {
    const int listen_fd = create_listen_socket();
    std::cout << "TinyWebServer listening on 0.0.0.0:" << port_ << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
            continue;
        }

        const std::string raw_request = read_request(client_fd);
        const HttpRequestLine request_line = parse_request_line(raw_request);
        const std::string response = build_response(request_line);
        send_all(client_fd, response);
        ::close(client_fd);
    }
}

int HttpServer::create_listen_socket() const {
    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw make_system_error("socket");
    }

    int opt = 1;
    if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ::close(listen_fd);
        throw make_system_error("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (::bind(listen_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd);
        throw make_system_error("bind");
    }

    if (::listen(listen_fd, kBacklog) < 0) {
        ::close(listen_fd);
        throw make_system_error("listen");
    }

    return listen_fd;
}

std::string HttpServer::read_request(int client_fd) {
    std::string request;
    request.reserve(kReadBufferSize);

    char buffer[kReadBufferSize];
    while (request.find("\r\n\r\n") == std::string::npos) {
        const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }
        request.append(buffer, static_cast<std::size_t>(n));
        if (request.size() > 64 * 1024) {
            break;
        }
    }

    return request;
}

HttpServer::HttpRequestLine HttpServer::parse_request_line(const std::string& raw_request) {
    HttpRequestLine request_line;

    const std::size_t line_end = raw_request.find("\r\n");
    if (line_end == std::string::npos) {
        return request_line;
    }

    std::istringstream line_stream(raw_request.substr(0, line_end));
    if (!(line_stream >> request_line.method >> request_line.path >> request_line.version)) {
        return request_line;
    }

    request_line.valid = true;
    return request_line;
}

std::string HttpServer::build_response(const HttpRequestLine& request_line) {
    if (!request_line.valid) {
        const std::string body = "Bad Request\n";
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Connection: close\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    if (request_line.method != "GET") {
        const std::string body = "Method Not Allowed\n";
        return "HTTP/1.1 405 Method Not Allowed\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Connection: close\r\n"
               "Allow: GET\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    const std::string body = "Hello from TinyWebServer\n"
                             "method=" + request_line.method + "\n"
                             "path=" + request_line.path + "\n"
                             "version=" + request_line.version + "\n";

    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/plain; charset=utf-8\r\n"
           "Connection: close\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

void HttpServer::send_all(int fd, const std::string& data) {
    std::size_t sent_total = 0;
    while (sent_total < data.size()) {
        const ssize_t sent = ::send(fd, data.data() + sent_total, data.size() - sent_total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
}

