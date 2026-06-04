#include "http_server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
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
//
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

        char ip_buf[INET_ADDRSTRLEN]{};
        ::inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
        const std::string client_ip(ip_buf);

        const HttpRequest request = read_request(client_fd);
        const std::string response = build_response(request, client_ip);
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

HttpServer::HttpRequest HttpServer::read_request(int client_fd) {
    std::string raw_headers;
    raw_headers.reserve(kReadBufferSize);

    char buffer[kReadBufferSize];
    // Read until we find the header/body separator
    while (raw_headers.find("\r\n\r\n") == std::string::npos) {
        const ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        raw_headers.append(buffer, static_cast<std::size_t>(n));
        if (raw_headers.size() > 64 * 1024) break;
    }

    HttpRequest request;
    request.request_line = parse_request_line(raw_headers);
    request.headers = parse_headers(raw_headers);

    // Determine Content-Length and read body if present
    std::size_t content_length = 0;
    auto cl_it = request.headers.find("content-length");
    if (cl_it != request.headers.end()) {
        try {
            content_length = std::stoull(cl_it->second);
        } catch (...) {
            content_length = 0;
        }
    }

    if (content_length > 0) {
        // Some body data may already be in raw_headers after \r\n\r\n
        const auto header_end_pos = raw_headers.find("\r\n\r\n");
        std::string already_read;
        if (header_end_pos != std::string::npos) {
            already_read = raw_headers.substr(header_end_pos + 4);
        }
        request.body = read_body(client_fd, already_read, content_length);
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

std::unordered_map<std::string, std::string> HttpServer::parse_headers(const std::string& raw_request) {
    std::unordered_map<std::string, std::string> headers;

    const auto line_end = raw_request.find("\r\n");
    if (line_end == std::string::npos) return headers;

    std::string header_section = raw_request;
    const auto body_start = raw_request.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        header_section = raw_request.substr(0, body_start);
    }

    std::istringstream stream(header_section.substr(line_end + 2));
    std::string line;
    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) break;

        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // Trim leading whitespace from value
        const auto val_start = value.find_first_not_of(' ');
        if (val_start != std::string::npos) {
            value = value.substr(val_start);
        } else {
            value.clear();
        }

        // Lowercase key for case-insensitive lookup
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        headers[key] = value;
    }

    return headers;
}

std::string HttpServer::read_body(int client_fd, const std::string& already_read, std::size_t content_length) {
    std::string body = already_read;
    body.reserve(content_length);

    while (body.size() < content_length) {
        char buf[kReadBufferSize];
        const std::size_t remaining = content_length - body.size();
        const std::size_t to_read = std::min(remaining, sizeof(buf));
        const ssize_t n = ::recv(client_fd, buf, to_read, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        body.append(buf, static_cast<std::size_t>(n));
    }

    return body;
}

bool HttpServer::is_image_content_type(const std::string& content_type) {
    // Check for image MIME types: image/jpeg, image/png, image/gif, image/webp, image/svg+xml, etc.
    std::string lower_ct = content_type;
    std::transform(lower_ct.begin(), lower_ct.end(), lower_ct.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // For multipart/form-data, we check the filename extension in the body instead
    // But also check if the top-level Content-Type hints at an image
    if (lower_ct.find("image/") != std::string::npos) {
        return true;
    }
    return false;
}

std::string HttpServer::build_response(const HttpRequest& request, const std::string& client_ip) {
    const auto& request_line = request.request_line;

    if (!request_line.valid) {
        const std::string body = "Bad Request\n";
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Connection: close\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    // Handle POST requests for file/image uploads
    if (request_line.method == "POST") {
        auto ct_it = request.headers.find("content-type");
        std::string content_type = (ct_it != request.headers.end()) ? ct_it->second : "";

        bool is_image = false;

        // Check top-level Content-Type for image
        if (is_image_content_type(content_type)) {
            is_image = true;
        }

        // For multipart/form-data, check the body for image content types in part headers
        std::string lower_ct = content_type;
        std::transform(lower_ct.begin(), lower_ct.end(), lower_ct.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_ct.find("multipart/form-data") != std::string::npos) {
            // Search the body for Content-Type: image/* in part headers
            std::string lower_body = request.body;
            std::transform(lower_body.begin(), lower_body.end(), lower_body.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower_body.find("content-type: image/") != std::string::npos) {
                is_image = true;
            }
        }

        if (is_image) {
            const std::string body = "image check=" + client_ip + "\n";
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain; charset=utf-8\r\n"
                   "Connection: close\r\n"
                   "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        }

        // Treat as generic file upload
        if (!request.body.empty()) {
            const std::string body = "file check=" + client_ip + "\n";
            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/plain; charset=utf-8\r\n"
                   "Connection: close\r\n"
                   "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        }

        // POST with no body
        const std::string body = "file check=" + client_ip + "\n";
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Connection: close\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    if (request_line.method != "GET") {
        const std::string body = "Method Not Allowed\n";
        return "HTTP/1.1 405 Method Not Allowed\r\n"
               "Content-Type: text/plain; charset=utf-8\r\n"
               "Connection: close\r\n"
               "Allow: GET, POST\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    const std::string body = "你好\n";

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

