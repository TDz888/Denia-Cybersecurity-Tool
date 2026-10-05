#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string url_encode(const std::string& in) {
    std::ostringstream oss;
    for (unsigned char c : in) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') oss << c;
        else oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

static std::string url_decode(const std::string& in) {
    std::string out;
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int v = std::stoi(in.substr(i + 1, 2), nullptr, 16);
            out += static_cast<char>(v);
            i += 2;
        } else if (in[i] == '+') out += ' ';
        else out += in[i];
    }
    return out;
}

static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string b64_encode(const std::string& in) {
    std::string out;
    int val = 0, bits = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out += B64[(val >> bits) & 0x3F];
            bits -= 6;
        }
    }
    if (bits > -6) out += B64[((val << 8) >> (bits + 8)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

static std::string b64_decode(const std::string& in) {
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        const char* p = std::strchr(B64, c);
        if (!p) continue;
        val = (val << 6) + static_cast<int>(p - B64);
        bits += 6;
        if (bits >= 0) {
            out += static_cast<char>((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

static std::string hex_encode(const std::string& in) {
    std::ostringstream oss;
    for (unsigned char c : in)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

static std::string hex_decode(const std::string& in) {
    std::string out;
    for (std::size_t i = 0; i + 1 < in.size(); i += 2) {
        int v = std::stoi(in.substr(i, 2), nullptr, 16);
        out += static_cast<char>(v);
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <op> <input>\n";
        std::cout << "Ops:\n"
                  << "  url-encode  url-decode\n"
                  << "  b64-encode  b64-decode\n"
                  << "  hex-encode  hex-decode\n";
        return 1;
    }
    std::string op = argv[1];
    std::string in = argv[2];

    if (op == "url-encode") std::cout << url_encode(in) << "\n";
    else if (op == "url-decode") std::cout << url_decode(in) << "\n";
    else if (op == "b64-encode") std::cout << b64_encode(in) << "\n";
    else if (op == "b64-decode") std::cout << b64_decode(in) << "\n";
    else if (op == "hex-encode") std::cout << hex_encode(in) << "\n";
    else if (op == "hex-decode") std::cout << hex_decode(in) << "\n";
    else { std::cerr << "Unknown op: " << op << "\n"; return 1; }
    return 0;
}
