#include <openssl/evp.h>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string to_hex(const unsigned char* data, unsigned int len) {
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    return oss.str();
}

static std::string hash_string(const std::string& in, const EVP_MD* md) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, nullptr);
    EVP_DigestUpdate(ctx, in.data(), in.size());
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, out, &len);
    EVP_MD_CTX_free(ctx);
    return to_hex(out, len);
}

static std::string hash_file(const std::string& path, const EVP_MD* md) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, md, nullptr);
    char buf[65536];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf, static_cast<size_t>(f.gcount()));
    }
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, out, &len);
    EVP_MD_CTX_free(ctx);
    return to_hex(out, len);
}

static void print_all(const std::string& label, const std::string& value) {
    std::cout << label << ":\n"
              << "  MD5    : " << value << "\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " text <string>\n";
        std::cout << "  " << argv[0] << " file <path>\n";
        std::cout << "  " << argv[0] << " crack <hash> <wordlist>\n";
        return 1;
    }
    std::string cmd = argv[1];

    if (cmd == "text" || cmd == "file") {
        std::string md5, sha1, sha256;
        std::string label;
        if (cmd == "text") {
            md5 = hash_string(argv[2], EVP_md5());
            sha1 = hash_string(argv[2], EVP_sha1());
            sha256 = hash_string(argv[2], EVP_sha256());
            label = "String: " + std::string(argv[2]);
        } else {
            md5 = hash_file(argv[2], EVP_md5());
            sha1 = hash_file(argv[2], EVP_sha1());
            sha256 = hash_file(argv[2], EVP_sha256());
            label = "File: " + std::string(argv[2]);
        }
        std::cout << label << "\n"
                  << "MD5    : " << md5 << "\n"
                  << "SHA1   : " << sha1 << "\n"
                  << "SHA256 : " << sha256 << "\n";
        return 0;
    }

    if (cmd == "crack") {
        if (argc < 4) { std::cerr << "Need wordlist\n"; return 1; }
        std::string target = argv[2];
        for (auto& c : target) c = std::tolower(c);
        std::ifstream f(argv[3]);
        if (!f) { std::cerr << "Cannot open wordlist\n"; return 1; }
        std::string word;
        long long tries = 0;
        while (std::getline(f, word)) {
            ++tries;
            if (!word.empty() && word.back() == '\r') word.pop_back();
            if (hash_string(word, EVP_md5()) == target) {
                std::cout << "[CRACKED] " << word << " (after " << tries << " tries)\n";
                return 0;
            }
        }
        std::cout << "Not found in " << tries << " words.\n";
        return 2;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}
