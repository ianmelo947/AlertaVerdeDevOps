#ifndef JSON_LITE_HPP
#define JSON_LITE_HPP

#include <string>
#include <unordered_map>
#include <regex>
#include <iostream>
#include <cctype>

namespace nlohmann {

class json {
public:
    std::unordered_map<std::string, std::string> kv;

    static json parse(const std::string &s) {
        json j;
        std::string clean_str = s;
        
        // Remove espaços em branco desnecessários
        clean_str.erase(0, clean_str.find_first_not_of(" \t\n\r"));
        clean_str.erase(clean_str.find_last_not_of(" \t\n\r") + 1);
        
        // Verifica se é um objeto JSON válido
        if (clean_str.front() != '{' || clean_str.back() != '}') {
            std::cerr << "Invalid JSON: must start with { and end with }" << std::endl;
            return j;
        }
        
        // Remove as chaves
        clean_str = clean_str.substr(1, clean_str.length() - 2);
        
        // Regex que captura tanto valores entre aspas quanto numéricos/booleanos
        std::regex reg("\"([^\"]+)\"\\s*:\\s*(\"([^\"]*)\"|([^,}\\]]+))");
        auto begin = std::sregex_iterator(clean_str.begin(), clean_str.end(), reg);
        auto end = std::sregex_iterator();
        
        for (auto it = begin; it != end; ++it) {
            if (it->size() >= 2) {
                std::string key = (*it)[1];
                std::string value;
                
                // Verifica se o valor está entre aspas (grupo 3) ou não (grupo 4)
                if (!(*it)[3].str().empty()) {
                    value = (*it)[3]; // Valor entre aspas
                } else {
                    value = (*it)[4]; // Valor sem aspas (numérico/boolean)
                    // Remove espaços extras do valor sem aspas
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                }
                
                j.kv[key] = value;
            }
        }
        
        return j;
    }

    std::string value(const std::string &key, const std::string &def) const {
        auto it = kv.find(key);
        return (it != kv.end() ? it->second : def);
    }

    size_t value(const std::string &key, size_t def) const {
        auto it = kv.find(key);
        if (it != kv.end() && !it->second.empty()) {
            try {
                return std::stoul(it->second);
            } catch (const std::exception& e) {
                return def;
            }
        }
        return def;
    }

    std::string dump() const {
        std::string out = "{";
        bool first = true;
        for (auto &p : kv) {
            if (!first) out += ",";
            first = false;
            
            // Sempre coloca aspas na chave
            out += "\"" + p.first + "\":";
            
            // Verifica se o valor é numérico
            bool is_numeric = !p.second.empty();
            bool has_decimal = false;
            for (size_t i = 0; i < p.second.length() && is_numeric; ++i) {
                char c = p.second[i];
                if (i == 0 && c == '-') continue; // Permite sinal negativo
                if (c == '.' && !has_decimal) {
                    has_decimal = true;
                    continue;
                }
                if (!std::isdigit(c)) {
                    is_numeric = false;
                }
            }
            
            // Verifica se é booleano
            bool is_boolean = (p.second == "true" || p.second == "false");
            
            if (is_numeric || is_boolean) {
                out += p.second; // Sem aspas para números e booleanos
            } else {
                // Escape de aspas no valor string
                std::string escaped_value;
                for (char c : p.second) {
                    if (c == '"') escaped_value += "\\\"";
                    else if (c == '\\') escaped_value += "\\\\";
                    else escaped_value += c;
                }
                out += "\"" + escaped_value + "\"";
            }
        }
        out += "}";
        return out;
    }

    std::string &operator[](const std::string &key) {
        return kv[key];
    }
    
    bool contains(const std::string &key) const {
        return kv.find(key) != kv.end();
    }
};

}

#endif