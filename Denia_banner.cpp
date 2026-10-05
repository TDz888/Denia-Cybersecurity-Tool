#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

static int connect_host(const std::string& host, int port, int timeout_ms) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char portstr[16];
    std::snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0 || !res) return -1;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return sock;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <host> <port> [timeout_ms]\n";
        return 1;
    }
    std::string host = argv[1];
    int port = std::atoi(argv[2]);
    int timeout_ms = (argc >= 4) ? std::atoi(argv[3]) : 3000;

    int sock = connect_host(host, port, timeout_ms);
    if (sock < 0) { std::cerr << "Cannot connect to " << host << ":" << port << "\n"; return 1; }

    if (port == 80 || port == 8080 || port == 8000) {
        std::string req = "HEAD / HTTP/1.0\r\nHost: " + host + "\r\n\r\n";
        send(sock, req.c_str(), req.size(), 0);
    }

    char buf[4096];
    std::string banner;
    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n > 0) banner.assign(buf, n);
    close(sock);

    if (banner.empty()) { std::cout << "(no banner)\n"; return 0; }
    for (char c : banner) {
        if (c == '\r') continue;
        std::cout << c;
    }
    if (banner.back() != '\n') std::cout << "\n";
    return 0;
}
