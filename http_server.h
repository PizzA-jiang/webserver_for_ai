#pragma once

#include <string>

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

    int port_;

    [[nodiscard]] int create_listen_socket() const;
    static std::string read_request(int client_fd);
    static HttpRequestLine parse_request_line(const std::string& raw_request);
    static std::string build_response(const HttpRequestLine& request_line);
    static void send_all(int fd, const std::string& data);
};
