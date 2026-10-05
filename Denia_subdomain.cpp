#include <netdb.h>
#include <arpa/inet.h>

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

std::mutex g_mtx;
std::atomic<int> g_found{0};

static bool resolve4(const std::string& host, std::string& ip) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr, buf, sizeof(buf));
    ip = buf;
    freeaddrinfo(res);
    return true;
}

static void worker(const std::string& domain, std::queue<std::string>* q) {
    while (true) {
        std::string sub;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            if (q->empty()) return;
            sub = q->front();
            q->pop();
        }
        std::string fqdn = sub + "." + domain;
        std::string ip;
        if (resolve4(fqdn, ip)) {
            std::lock_guard<std::mutex> lk(g_mtx);
            std::cout << "[FOUND] " << fqdn << " -> " << ip << "\n";
            g_found.fetch_add(1);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <domain> <wordlist> [threads]\n";
        std::cout << "Example: " << argv[0] << " example.com subs.txt 50\n";
        return 1;
    }
    std::string domain = argv[1];
    std::string listfile = argv[2];
    int threads = (argc >= 4) ? std::atoi(argv[3]) : 50;
    if (threads < 1) threads = 1;
    if (threads > 500) threads = 500;

    std::ifstream in(listfile);
    if (!in) { std::cerr << "Cannot open " << listfile << "\n"; return 1; }

    std::queue<std::string> q;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) q.push(line);
    }
    int total = static_cast<int>(q.size());
    std::cout << "Enumerating " << total << " subdomains for " << domain
              << " with " << threads << " threads\n\n";

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) pool.emplace_back(worker, domain, &q);
    for (auto& t : pool) t.join();

    std::cout << "\nFound " << g_found.load() << " subdomains.\n";
    return 0;
}
