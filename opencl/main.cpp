#include <iostream>
#include <sqlite3.h>
#include <bcrypt/BCrypt.hpp>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

sqlite3* db = nullptr;

bool initDatabase() {
    int rc = sqlite3_open("/data/db.sqlite", &db);
    if (rc) {
        cerr << "Erro ao abrir banco de dados: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "email TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        
        "CREATE TABLE IF NOT EXISTS crops ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "planting_date DATE NOT NULL,"
        "area REAL NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY(user_id) REFERENCES users(id)"
        ");";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        cerr << "Erro SQL: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    cout << "Banco de dados inicializado com sucesso!" << endl;
    return true;
}

string generateToken() {
    // Token simples - em produção use JWT
    const string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[rand() % chars.length()];
    }
    return token;
}

int main() {
    srand(time(nullptr));

    if (!initDatabase()) {
        return 1;
    }

    httplib::Server svr;

    // Rota de registro
    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto js = json::parse(req.body);
            string name = js["name"];
            string email = js["email"];
            string password = js["password"];

            // Hash da senha
            string hashedPassword = BCrypt::generateHash(password, 12);

            const char* sql = "INSERT INTO users (name, email, password_hash) VALUES (?, ?, ?)";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                throw runtime_error("Erro ao preparar SQL");
            }

            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, hashedPassword.c_str(), -1, SQLITE_STATIC);

            json response;
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                response["success"] = true;
                response["message"] = "Usuário cadastrado com sucesso";
            } else {
                response["success"] = false;
                response["message"] = "E-mail já cadastrado";
            }

            sqlite3_finalize(stmt);
            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json response;
            response["success"] = false;
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Rota de login
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto js = json::parse(req.body);
            string email = js["email"];
            string password = js["password"];

            const char* sql = "SELECT id, name, email, password_hash FROM users WHERE email = ?";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                throw runtime_error("Erro ao preparar SQL");
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            json response;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                
                if (BCrypt::validatePassword(password, storedHash)) {
                    response["success"] = true;
                    response["token"] = generateToken();
                    response["user"] = {
                        {"id", sqlite3_column_int(stmt, 0)},
                        {"name", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))},
                        {"email", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))}
                    };
                } else {
                    response["success"] = false;
                    response["message"] = "Senha incorreta";
                }
            } else {
                response["success"] = false;
                response["message"] = "Usuário não encontrado";
            }

            sqlite3_finalize(stmt);
            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json response;
            response["success"] = false;
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Rota para obter culturas do usuário
    svr.Get("/api/crops", [](const httplib::Request& req, httplib::Response& res) {
        // Simulação - em produção, verifique o token e obtenha o user_id
        const char* sql = "SELECT id, name, type, planting_date, area FROM crops WHERE user_id = 1";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            res.status = 500;
            return;
        }

        json crops = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            crops.push_back({
                {"id", sqlite3_column_int(stmt, 0)},
                {"name", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))},
                {"type", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))},
                {"plantingDate", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))},
                {"area", sqlite3_column_double(stmt, 4)}
            });
        }

        sqlite3_finalize(stmt);
        
        json response;
        response["crops"] = crops;
        res.set_content(response.dump(), "application/json");
    });

    // Rota para adicionar cultura
    svr.Post("/api/crops", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto js = json::parse(req.body);
            string name = js["name"];
            string type = js["type"];
            string plantingDate = js["plantingDate"];
            double area = js["area"];

            const char* sql = "INSERT INTO crops (user_id, name, type, planting_date, area) VALUES (1, ?, ?, ?, ?)";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                throw runtime_error("Erro ao preparar SQL");
            }

            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, plantingDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 4, area);

            json response;
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                response["success"] = true;
                response["message"] = "Cultura cadastrada com sucesso";
            } else {
                response["success"] = false;
                response["message"] = "Erro ao cadastrar cultura";
            }

            sqlite3_finalize(stmt);
            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json response;
            response["success"] = false;
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });

    // Rota para deletar cultura
    svr.Delete(R"(/api/crops/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int cropId = stoi(req.matches[1]);
            
            const char* sql = "DELETE FROM crops WHERE id = ?";
            sqlite3_stmt* stmt;
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                throw runtime_error("Erro ao preparar SQL");
            }

            sqlite3_bind_int(stmt, 1, cropId);

            json response;
            if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0) {
                response["success"] = true;
                response["message"] = "Cultura removida com sucesso";
            } else {
                response["success"] = false;
                response["message"] = "Cultura não encontrada";
            }

            sqlite3_finalize(stmt);
            res.set_content(response.dump(), "application/json");

        } catch (exception& e) {
            json response;
            response["success"] = false;
            response["message"] = e.what();
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });

    cout << "Servidor rodando na porta 8080..." << endl;
    svr.listen("0.0.0.0", 8080);

    sqlite3_close(db);
    return 0;
}