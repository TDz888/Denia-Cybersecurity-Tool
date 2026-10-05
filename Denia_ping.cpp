#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

std::atomic<bool> g_running{true};
void on_sigint(int) { g_running.store(false); }

static uint16_t icmp_checksum(const void* data, std::size_t len) {
    const auto* p = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len == 1) sum += *reinterpret_cast<const uint8_t*>(p);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

static bool resolve_host(const std::string& host, sockaddr_in& out, std::string& ip) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    addrinfo* res = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || !res) return false;
    out = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &out.sin_addr, buf, sizeof(buf));
    ip = buf;
    freeaddrinfo(res);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <host> [count] [interval_ms]\n";
        return 1;
    }
    std::string host = argv[1];
    int count = (argc >= 3) ? std::atoi(argv[2]) : 4;
    int interval_ms = (argc >= 4) ? std::atoi(argv[3]) : 1000;
    if (count <= 0) count = 4;
    if (interval_ms < 100) interval_ms = 100;

    sockaddr_in target{};
    std::string ip;
    if (!resolve_host(host, target, ip)) {
        std::cerr << "Cannot resolve " << host << "\n";
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        std::cerr << "socket: " << std::strerror(errno) << "\n";
        return 1;
    }

    timeval tv{2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    std::signal(SIGINT, on_sigint);

    std::cout << "PING " << host << " (" << ip << ") count=" << count << "\n\n";

    const uint16_t id = static_cast<uint16_t>(getpid() & 0xFFFF);
    std::vector<double> rtts;
    int sent = 0, received = 0;

    for (int seq = 1; seq <= count && g_running.load(); ++seq) {
        uint8_t packet[64];
        std::memset(packet, 0, sizeof(packet));
        auto* hdr = reinterpret_cast<icmphdr*>(packet);
        hdr->type = ICMP_ECHO;
        hdr->code = 0;
        hdr->un.echo.id = htons(id);
        hdr->un.echo.sequence = htons(static_cast<uint16_t>(seq));
        hdr->checksum = icmp_checksum(packet, sizeof(packet));

        auto t0 = std::chrono::steady_clock::now();
        ssize_t s = sendto(sock, packet, sizeof(packet), 0,
                           reinterpret_cast<sockaddr*>(&target), sizeof(target));
        ++sent;
        if (s < 0) {
            std::cout << "seq=" << seq << " send error\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            continue;
        }

        uint8_t buf[1024];
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             reinterpret_cast<sockaddr*>(&from), &from_len);
        auto t1 = std::chrono::steady_clock::now();

        if (n < 0) {
            std::cout << "seq=" << seq << " timeout\n";
        } else {
            std::size_t ip_len = 0;
            if (n >= static_cast<ssize_t>(sizeof(ip))) {
                auto* iph = reinterpret_cast<ip*>(buf);
                if (iph->ip_v == 4) ip_len = static_cast<std::size_t>(iph->ip_hl) * 4;
            }
            if (n >= static_cast<ssize_t>(ip_len + sizeof(icmphdr))) {
                auto* reply = reinterpret_cast<icmphdr*>(buf + ip_len);
                if (reply->type == ICMP_ECHOREPLY) {
                    double rtt = std::chrono::duration<double, std::milli>(t1 - t0).count();
                    rtts.push_back(rtt);
                    ++received;
                    std::cout << "64 bytes from " << inet_ntoa(from.sin_addr)
                              << ": icmp_seq=" << seq
                              << " time=" << std::fixed << std::setprecision(2) << rtt
                              << " ms\n";
                }
            }
        }
        if (seq < count && g_running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    close(sock);

    int lost = sent - received;
    double loss = sent ? 100.0 * lost / sent : 0.0;
    std::cout << "\n--- " << host << " ping statistics ---\n";
    std::cout << sent << " sent, " << received << " received, "
              << std::fixed << std::setprecision(1) << loss << "% loss\n";
    if (!rtts.empty()) {
        auto [mn, mx] = std::minmax_element(rtts.begin(), rtts.end());
        double sum = 0;
        for (double v : rtts) sum += v;
        double avg = sum / rtts.size();
        double var = 0;
        for (double v : rtts) var += (v - avg) * (v - avg);
        double mdev = std::sqrt(var / rtts.size());
        std::cout << "rtt min/avg/max/mdev = " << std::fixed << std::setprecision(2)
                  << *mn << "/" << avg << "/" << *mx << "/" << mdev << " ms\n";
    }
    return received > 0 ? 0 : 2;
}
