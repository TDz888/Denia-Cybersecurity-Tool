#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <arpa/nameser.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static const char* type_name(int t) {
    switch (t) {
        case ns_t_a: return "A";
        case ns_t_aaaa: return "AAAA";
        case ns_t_mx: return "MX";
        case ns_t_txt: return "TXT";
        case ns_t_ns: return "NS";
        case ns_t_cname: return "CNAME";
        case ns_t_ptr: return "PTR";
        case ns_t_soa: return "SOA";
        default: return "?";
    }
}

static void query(const std::string& domain, int type) {
    unsigned char buf[4096];
    int len = res_query(domain.c_str(), ns_c_in, type, buf, sizeof(buf));
    if (len < 0) {
        std::cout << "[" << type_name(type) << "] no record\n";
        return;
    }
    ns_msg msg;
    if (ns_initparse(buf, len, &msg) < 0) return;
    int count = ns_msg_count(msg, ns_s_an);
    for (int i = 0; i < count; ++i) {
        ns_rr rr;
        if (ns_parserr(&msg, ns_s_an, i, &rr) < 0) continue;
        const char* name = ns_rr_name(rr);
        int t = ns_rr_type(rr);
        const uint8_t* rd = ns_rr_rdata(rr);
        char out[1024] = {0};

        if (t == ns_t_a) {
            inet_ntop(AF_INET, rd, out, sizeof(out));
        } else if (t == ns_t_aaaa) {
            inet_ntop(AF_INET6, rd, out, sizeof(out));
        } else if (t == ns_t_mx) {
            char host[256] = {0};
            dn_expand(ns_msg_base(msg), ns_msg_end(msg), rd + 2, host, sizeof(host));
            std::snprintf(out, sizeof(out), "%u %s", ntohs(*reinterpret_cast<const uint16_t*>(rd)), host);
        } else if (t == ns_t_txt) {
            std::size_t off = 0;
            for (int k = 0; k < rd[0] && off + 1 < sizeof(out); ++k)
                out[off++] = static_cast<char>(rd[k + 1]);
        } else if (t == ns_t_ns || t == ns_t_cname || t == ns_t_ptr) {
            dn_expand(ns_msg_base(msg), ns_msg_end(msg), rd, out, sizeof(out));
        } else if (t == ns_t_soa) {
            char mname[256] = {0}, rname[256] = {0};
            const uint8_t* p = rd;
            p += dn_expand(ns_msg_base(msg), ns_msg_end(msg), p, mname, sizeof(mname));
            p += dn_expand(ns_msg_base(msg), ns_msg_end(msg), p, rname, sizeof(rname));
            std::snprintf(out, sizeof(out), "%s %s", mname, rname);
        }
        std::cout << name << "\t" << type_name(t) << "\t" << out << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <domain> [type]\n";
        std::cout << "Types: A AAAA MX TXT NS CNAME SOA PTR ALL\n";
        return 1;
    }
    std::string domain = argv[1];
    std::string type = (argc >= 3) ? argv[2] : "ALL";
    res_init();

    std::cout << ";; Domain: " << domain << "\n";
    if (type == "ALL") {
        for (int t : {ns_t_a, ns_t_aaaa, ns_t_mx, ns_t_txt, ns_t_ns, ns_t_cname, ns_t_soa})
            query(domain, t);
    } else if (type == "A") query(domain, ns_t_a);
    else if (type == "AAAA") query(domain, ns_t_aaaa);
    else if (type == "MX") query(domain, ns_t_mx);
    else if (type == "TXT") query(domain, ns_t_txt);
    else if (type == "NS") query(domain, ns_t_ns);
    else if (type == "CNAME") query(domain, ns_t_cname);
    else if (type == "SOA") query(domain, ns_t_soa);
    else if (type == "PTR") query(domain, ns_t_ptr);
    else { std::cerr << "Unknown type: " << type << "\n"; return 1; }
    return 0;
}
