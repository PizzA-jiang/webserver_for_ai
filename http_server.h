#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

class HttpServer {
public:
    explicit HttpServer(int port);

    // Starts the blocking accept loop.
    void run();

private:
    struct HttpRequestLine {
        std::string method;
        std::string path;
        std::string version;
        bool valid{false};
    };

    struct HttpRequest {
        HttpRequestLine request_line;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    int port_;

    [[nodiscard]] int create_listen_socket() const;
    static HttpRequest read_request(int client_fd);
    static HttpRequestLine parse_request_line(const std::string& raw_request);
    static std::unordered_map<std::string, std::string> parse_headers(const std::string& raw_request);
    static std::string read_body(int client_fd, const std::string& raw_request, std::size_t content_length);
    static bool is_image_content_type(const std::string& content_type);
    static std::string build_response(const HttpRequest& request, const std::string& client_ip);
    static void send_all(int fd, const std::string& data);
};
