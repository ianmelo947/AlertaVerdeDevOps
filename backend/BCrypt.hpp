#ifndef BCRYPT_HPP
#define BCRYPT_HPP

#include <string>
#include <cstring>
#include <openssl/rand.h>
#include <openssl/evp.h>

namespace BCrypt {

std::string generateHash(const std::string& password, int workFactor = 12) {
    // Implementação simplificada - em produção use uma biblioteca bcrypt completa
    // Esta é uma versão básica apenas para demonstração
    unsigned char salt[16];
    RAND_bytes(salt, sizeof(salt));
    
    unsigned char hash[32];
    PKCS5_PBKDF2_HMAC_SHA1(password.c_str(), password.length(),
                          salt, sizeof(salt), 1 << workFactor,
                          sizeof(hash), hash);
    
    std::string result;
    char buffer[3];
    for (int i = 0; i < sizeof(salt); i++) {
        snprintf(buffer, sizeof(buffer), "%02x", salt[i]);
        result += buffer;
    }
    result += ":";
    for (int i = 0; i < sizeof(hash); i++) {
        snprintf(buffer, sizeof(buffer), "%02x", hash[i]);
        result += buffer;
    }
    
    return result;
}

bool validatePassword(const std::string& password, const std::string& hash) {
    size_t pos = hash.find(':');
    if (pos == std::string::npos) return false;
    
    std::string salt_hex = hash.substr(0, pos);
    std::string stored_hash_hex = hash.substr(pos + 1);
    
    // Converter hex para bytes
    unsigned char salt[16];
    for (size_t i = 0; i < salt_hex.length(); i += 2) {
        sscanf(salt_hex.substr(i, 2).c_str(), "%02hhx", &salt[i/2]);
    }
    
    unsigned char computed_hash[32];
    PKCS5_PBKDF2_HMAC_SHA1(password.c_str(), password.length(),
                          salt, sizeof(salt), 1 << 12, // work factor 12
                          sizeof(computed_hash), computed_hash);
    
    // Converter hash computado para hex
    std::string computed_hash_hex;
    char buffer[3];
    for (int i = 0; i < sizeof(computed_hash); i++) {
        snprintf(buffer, sizeof(buffer), "%02x", computed_hash[i]);
        computed_hash_hex += buffer;
    }
    
    return computed_hash_hex == stored_hash_hex;
}

} // namespace BCrypt

#endif