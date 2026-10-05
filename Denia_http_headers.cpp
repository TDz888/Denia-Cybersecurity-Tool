#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

struct Url {
    std::string scheme, host, path;
    int port;
};

static bool parse_url(const std::string& in, Url& u) {
    std::string s = in;
    auto pos = s.find("://");
    if (pos == std::string::npos) { u.scheme = "http"; }
    else { u.scheme = s.substr(0, pos); s = s.substr(pos + 3); }

    u.port = (u.scheme == "https") ? 443 : 80;

    auto slash = s.find('/');
    if (slash == std::string::npos) { u.host = s; u.path = "/"; }
    else { u.host = s.substr(0, slash); u.path = s.substr(slash); }

    auto colon = u.host.find(':');
    if (colon != std::string::npos) {
        u.port = std::atoi(u.host.substr(colon + 1).c_str());
        u.host = u.host.substr(0, colon);
    }
    return !u.host.empty();
}

static int connect_host(const std::string& host, int port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char portstr[16];
    std::snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(host.c_str(), portstr, &hints, &res) != 0 || !res) return -1;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return sock;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <url>\n";
        std::cout << "Example: " << argv[0] << " http://example.com/\n";
        return 1;
    }
    Url u;
    if (!parse_url(argv[1], u)) { std::cerr << "Bad URL\n"; return 1; }
    if (u.scheme == "https") {
        std::cerr << "HTTPS not supported in this tool. Use denia_ssl_info.\n";
        return 1;
    }

    int sock = connect_host(u.host, u.port);
    if (sock < 0) { std::cerr << "Cannot connect\n"; return 1; }

    std::string req = "GET " + u.path + " HTTP/1.1\r\n"
                      "Host: " + u.host + "\r\n"
                      "User-Agent: Denia-Toolkit/1.0\r\n"
                      "Accept: */*\r\n"
                      "Connection: close\r\n\r\n";
    send(sock, req.c_str(), req.size(), 0);

    std::string resp;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        resp.append(buf, n);
        if (resp.size() > 1024 * 1024) break;
    }
    close(sock);

    auto hdr_end = resp.find("\r\n\r\n");
    if (hdr_end == std::string::npos) {
        std::cout << resp << "\n";
        return 0;
    }
    std::cout << resp.substr(0, hdr_end + 4);
    return 0;
}
