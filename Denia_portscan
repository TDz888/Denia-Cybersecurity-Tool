#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

std::mutex g_mtx;
std::atomic<int> g_open{0};
std::atomic<int> g_done{0};

static bool scan_port(const std::string& ip, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int rc = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc == 0) { close(sock); return true; }
    if (errno != EINPROGRESS) { close(sock); return false; }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    rc = select(sock + 1, nullptr, &wfds, nullptr, &tv);
    if (rc <= 0) { close(sock); return false; }

    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
    close(sock);
    return err == 0;
}

static void worker(const std::string& ip, std::queue<int>* ports, int timeout_ms) {
    while (true) {
        int port;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            if (ports->empty()) return;
            port = ports->front();
            ports->pop();
        }
        if (scan_port(ip, port, timeout_ms)) {
            std::lock_guard<std::mutex> lk(g_mtx);
            std::cout << "[OPEN] " << ip << ":" << port << "\n";
            g_open.fetch_add(1);
        }
        g_done.fetch_add(1);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <host> <start-end> [threads] [timeout_ms]\n";
        std::cout << "Example: " << argv[0] << " 192.168.1.1 1-1024 200 500\n";
        return 1;
    }
    std::string host = argv[1];
    int start = 1, end = 1024;
    if (std::sscanf(argv[2], "%d-%d", &start, &end) != 2) {
        start = end = std::atoi(argv[2]);
    }
    int threads = (argc >= 4) ? std::atoi(argv[3]) : 200;
    int timeout_ms = (argc >= 5) ? std::atoi(argv[4]) : 500;
    if (threads < 1) threads = 1;
    if (threads > 2000) threads = 2000;

    sockaddr_in tmp{};
    std::string ip = host;
    if (inet_pton(AF_INET, host.c_str(), &tmp.sin_addr) != 1) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            std::cerr << "Cannot resolve " << host << "\n";
            return 1;
        }
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr, buf, sizeof(buf));
        ip = buf;
        freeaddrinfo(res);
    }

    std::queue<int> ports;
    for (int p = start; p <= end; ++p) ports.push(p);
    int total = static_cast<int>(ports.size());

    std::cout << "Scanning " << ip << " ports " << start << "-" << end
              << " with " << threads << " threads\n";

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) pool.emplace_back(worker, ip, &ports, timeout_ms);
    for (auto& t : pool) t.join();
    auto t1 = std::chrono::steady_clock::now();

    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "\nScanned " << total << " ports in " << secs << "s. "
              << g_open.load() << " open.\n";
    return 0;
}
