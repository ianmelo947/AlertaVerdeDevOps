#define CL_TARGET_OPENCL_VERSION 220
#include <iostream>
#include <sqlite3.h>
#include <openssl/sha.h>
#include <regex>
#include <unordered_map>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <vector>

// BIBLIOTECA OPENCL
#include <CL/cl.h>

using namespace std;

sqlite3* db = nullptr;

// ==========================================
// CÓDIGO OPENCL (KERNEL)
// Este código é compilado em tempo de execução
// ==========================================
const char* kernelSource = R"(
__kernel void simular_colheita(__global const float* areas, 
                               __global const float* temperaturas,
                               __global float* resultados,
                               const int count) {
    
    // Descobre qual 'fio' de processamento somos
    int i = get_global_id(0);
    
    if (i < count) {
        float area = areas[i];
        float temp = temperaturas[i];
        
        // Lógica de simulação paralela:
        // Se a temperatura estiver entre 20 e 30, produtividade é 100% (1.0)
        // Se for muito quente ou frio, cai para 50% (0.5)
        float fator_clima = 0.5;
        if (temp >= 20.0 && temp <= 30.0) {
            fator_clima = 1.0;
        }
        
        // Produtividade base: 3kg por m2 * fator climático
        resultados[i] = area * 3.0 * fator_clima;
    }
}
)";

// ==========================================
// CLASSES UTILITÁRIAS
// ==========================================
class Json {
private:
    unordered_map<string, string> data;
public:
    void set(const string& key, const string& value) { data[key] = value; }
    string get(const string& key, const string& def = "") const {
        auto it = data.find(key);
        return (it != data.end() ? it->second : def);
    }
    static Json parse(const string& jsonStr) {
        Json result;
        regex reg("\"([^\"]+)\"\\s*:\\s*\"?([^\",}]+)\"?");
        auto begin = sregex_iterator(jsonStr.begin(), jsonStr.end(), reg);
        auto end = sregex_iterator();
        for(auto it = begin; it != end; ++it) {
            string key = (*it)[1];
            string value = (*it)[2];
            if (!value.empty() && value.back() == '"') value.pop_back();
            result.data[key] = value;
        }
        return result;
    }
    string dump() const {
        string out = "{";
        bool first = true;
        for (auto &p : data) {
            if (!first) out += ",";
            first = false;
            out += "\"" + p.first + "\":\"" + p.second + "\"";
        }
        out += "}";
        return out;
    }
};

string hashPassword(const string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)password.c_str(), password.length(), hash);
    string result;
    char buffer[3];
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(buffer, sizeof(buffer), "%02x", hash[i]);
        result += buffer;
    }
    return result;
}

string generateToken() { return "sessao_" + to_string(rand()); }

bool initDatabase() {
    int rc = sqlite3_open("/data/db.sqlite", &db);
    if (rc) return false;
    const char* sql = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, email TEXT UNIQUE, password_hash TEXT);"
                      "CREATE TABLE IF NOT EXISTS crops (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, name TEXT, type TEXT, planting_date TEXT, area REAL);";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    return true;
}

// HTTP Helpers
string getRequestBody(const string& req) {
    size_t pos = req.find("\r\n\r\n");
    return (pos == string::npos) ? "" : req.substr(pos + 4);
}
string getRequestMethod(const string& req) { stringstream ss(req); string m; ss >> m; return m; }
string getRequestPath(const string& req) { stringstream ss(req); string m, p; ss >> m >> p; return p; }

void sendResponse(int client, const string& body, int code = 200) {
    string msg = (code == 200) ? "OK" : "Error";
    stringstream ss;
    ss << "HTTP/1.1 " << code << " " << msg << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
       << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    string out = ss.str();
    send(client, out.c_str(), out.size(), 0);
}

// ==========================================
// FUNÇÃO DE SIMULAÇÃO COM OPENCL
// ==========================================
string runOpenCLSimulation(float area, float temp) {
    // Variáveis OpenCL
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;

    // 1. Configurar Ambiente (Platforma e Device)
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

    // 2. Criar Contexto
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    cl_command_queue command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &ret);

    // 3. Preparar Dados (Buffers de Memória)
    // Vamos simular apenas 1 item por enquanto, mas OpenCL suporta milhões
    const int SIZE = 1;
    cl_mem mem_areas = clCreateBuffer(context, CL_MEM_READ_ONLY, SIZE * sizeof(float), NULL, &ret);
    cl_mem mem_temps = clCreateBuffer(context, CL_MEM_READ_ONLY, SIZE * sizeof(float), NULL, &ret);
    cl_mem mem_res   = clCreateBuffer(context, CL_MEM_WRITE_ONLY, SIZE * sizeof(float), NULL, &ret);

    // Copiar dados da RAM (CPU) para o Buffer (OpenCL)
    ret = clEnqueueWriteBuffer(command_queue, mem_areas, CL_TRUE, 0, SIZE * sizeof(float), &area, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, mem_temps, CL_TRUE, 0, SIZE * sizeof(float), &temp, 0, NULL, NULL);

    // 4. Compilar o Programa (Kernel)
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernelSource, NULL, &ret);
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

    // Debug se falhar a compilação
    if (ret != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char *) malloc(log_size);
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        cout << "Erro Build OpenCL: " << log << endl;
        free(log);
        return "0";
    }

    cl_kernel kernel = clCreateKernel(program, "simular_colheita", &ret);

    // 5. Definir Argumentos
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&mem_areas);
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&mem_temps);
    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&mem_res);
    ret = clSetKernelArg(kernel, 3, sizeof(int), (void *)&SIZE);

    // 6. Executar Kernel
    size_t global_item_size = SIZE;
    size_t local_item_size = 1;
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);

    // 7. Ler Resultados de volta
    float resultado = 0;
    ret = clEnqueueReadBuffer(command_queue, mem_res, CL_TRUE, 0, SIZE * sizeof(float), &resultado, 0, NULL, NULL);

    // Limpeza
    clFlush(command_queue);
    clFinish(command_queue);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseMemObject(mem_areas);
    clReleaseMemObject(mem_temps);
    clReleaseMemObject(mem_res);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);

    return to_string(resultado);
}

// Handlers
void handleRegister(const string& body, int client) {
    Json js = Json::parse(body);
    string sql = "INSERT INTO users (name, email, password_hash) VALUES ('" + 
                 js.get("name") + "', '" + js.get("email") + "', '" + hashPassword(js.get("password")) + "');";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    sendResponse(client, "{\"success\":\"true\"}");
}

void handleLogin(const string& body, int client) {
    Json js = Json::parse(body);
    string sql = "SELECT name, email FROM users WHERE email='" + js.get("email") + 
                 "' AND password_hash='" + hashPassword(js.get("password")) + "'";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        Json res; res.set("success", "true"); res.set("token", generateToken());
        res.set("user_name", string((const char*)sqlite3_column_text(stmt, 0)));
        res.set("user_email", string((const char*)sqlite3_column_text(stmt, 1)));
        sendResponse(client, res.dump());
    } else {
        sendResponse(client, "{\"success\":\"false\"}");
    }
    sqlite3_finalize(stmt);
}

void handleGetCrops(int client) {
    sqlite3_stmt* stmt;
    string arr = "[";
    if (sqlite3_prepare_v2(db, "SELECT id, name, type, planting_date, area FROM crops", -1, &stmt, nullptr) == SQLITE_OK) {
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (!first) arr += ","; first = false;
            arr += "{\"id\":" + to_string(sqlite3_column_int(stmt, 0)) + ",";
            arr += "\"name\":\"" + string((const char*)sqlite3_column_text(stmt, 1)) + "\",";
            arr += "\"type\":\"" + string((const char*)sqlite3_column_text(stmt, 2)) + "\",";
            arr += "\"plantingDate\":\"" + string((const char*)sqlite3_column_text(stmt, 3)) + "\",";
            arr += "\"area\":\"" + to_string(sqlite3_column_double(stmt, 4)) + "\"}";
        }
    }
    sendResponse(client, "{\"crops\": " + arr + "]}");
    sqlite3_finalize(stmt);
}

void handleAddCrop(const string& body, int client) {
    Json js = Json::parse(body);
    string sql = "INSERT INTO crops (user_id, name, type, planting_date, area) VALUES (1, '" + 
                 js.get("name") + "', '" + js.get("type") + "', '" + js.get("plantingDate") + "', " + js.get("area") + ");";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    sendResponse(client, "{\"success\":\"true\"}");
}

void handleDeleteCrop(int id, int client) {
    sqlite3_exec(db, ("DELETE FROM crops WHERE id=" + to_string(id)).c_str(), nullptr, nullptr, nullptr);
    sendResponse(client, "{\"success\":\"true\"}");
}

void handleSimulation(const string& body, int client) {
    // 1. Ler dados do Frontend
    Json js = Json::parse(body);
    float area = stof(js.get("area"));
    float temp = stof(js.get("temp"));

    cout << "Iniciando Simulação OpenCL..." << endl;
    cout << "Area: " << area << " Temp: " << temp << endl;

    // 2. Chamar OpenCL
    string resultado = runOpenCLSimulation(area, temp);

    // 3. Retornar
    Json response;
    response.set("success", "true");
    response.set("producao_estimada", resultado);
    sendResponse(client, response.dump());
}

int main() {
    initDatabase();
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{AF_INET, htons(8080), {INADDR_ANY}};
    bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 10);
    cout << "Server + OpenCL rodando na porta 8080..." << endl;

    while(true) {
        int client = accept(sockfd, nullptr, nullptr);
        char buf[4096] = {0}; read(client, buf, 4096);
        string req(buf); if(req.empty()) { close(client); continue; }
        
        string m = getRequestMethod(req), p = getRequestPath(req), b = getRequestBody(req);
        
        if (m == "OPTIONS") sendResponse(client, "", 204);
        else if (p == "/api/register") handleRegister(b, client);
        else if (p == "/api/login") handleLogin(b, client);
        else if (p == "/api/crops" && m == "GET") handleGetCrops(client);
        else if (p == "/api/crops" && m == "POST") handleAddCrop(b, client);
        else if (p == "/api/simulation" && m == "POST") handleSimulation(b, client); // NOVA ROTA
        else if (p.find("/api/crops/") == 0 && m == "DELETE") handleDeleteCrop(stoi(p.substr(11)), client);
        else sendResponse(client, "{\"error\":\"Not Found\"}", 404);
        
        close(client);
    }
}