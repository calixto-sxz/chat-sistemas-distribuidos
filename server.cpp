#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clientes;
std::mutex clientesMutex;
std::vector<std::string> historico;
std::mutex historicoMutex;
const std::string arquivoHistorico = "historico.txt";

void carregarHistorico() {
    std::ifstream arquivo(arquivoHistorico);
    std::string linha;
    while (std::getline(arquivo, linha)) {
        historico.push_back(linha);
    }
}

void salvarNoArquivo(const std::string& mensagem) {
    std::ofstream arquivo(arquivoHistorico, std::ios::app);
    arquivo << mensagem << "\n";
}

void enviarMensagem(SOCKET socket, const std::string& mensagem) {
    std::string msg = mensagem + "\n";
    send(socket, msg.c_str(), (int)msg.size(), 0);
}

void broadcast(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(clientesMutex);
    for (SOCKET cliente : clientes) {
        enviarMensagem(cliente, mensagem);
    }
}

void removerCliente(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientesMutex);
    for (size_t i = 0; i < clientes.size(); i++) {
        if (clientes[i] == socket) {
            clientes.erase(clientes.begin() + i);
            break;
        }
    }
}

void tratarCliente(SOCKET socket) {
    char buffer[1024];
    int bytesRecebidos = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRecebidos <= 0) {
        closesocket(socket);
        return;
    }
    buffer[bytesRecebidos] = '\0';
    std::string nomeUsuario(buffer);
    while (!nomeUsuario.empty() && (nomeUsuario.back() == '\n' || nomeUsuario.back() == '\r')) {
        nomeUsuario.pop_back();
    }

    {
        std::lock_guard<std::mutex> lock(historicoMutex);
        for (const std::string& linha : historico) {
            enviarMensagem(socket, linha);
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientesMutex);
        clientes.push_back(socket);
    }

    std::string mensagemEntrada = nomeUsuario + " entrou no chat";
    {
        std::lock_guard<std::mutex> lock(historicoMutex);
        historico.push_back(mensagemEntrada);
        salvarNoArquivo(mensagemEntrada);
    }
    broadcast(mensagemEntrada);
    std::cout << mensagemEntrada << std::endl;

    while (true) {
        bytesRecebidos = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRecebidos <= 0) {
            break;
        }
        buffer[bytesRecebidos] = '\0';
        std::string texto(buffer);
        while (!texto.empty() && (texto.back() == '\n' || texto.back() == '\r')) {
            texto.pop_back();
        }
        if (texto.empty()) {
            continue;
        }
        std::string mensagemCompleta = nomeUsuario + ": " + texto;
        {
            std::lock_guard<std::mutex> lock(historicoMutex);
            historico.push_back(mensagemCompleta);
            salvarNoArquivo(mensagemCompleta);
        }
        std::cout << mensagemCompleta << std::endl;
        broadcast(mensagemCompleta);
    }

    std::string mensagemSaida = nomeUsuario + " saiu do chat";
    {
        std::lock_guard<std::mutex> lock(historicoMutex);
        historico.push_back(mensagemSaida);
        salvarNoArquivo(mensagemSaida);
    }
    removerCliente(socket);
    broadcast(mensagemSaida);
    std::cout << mensagemSaida << std::endl;
    closesocket(socket);
}

int main(int argc, char* argv[]) {
    int porta = 5000;
    if (argc > 1) {
        porta = std::stoi(argv[1]);
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET socketServidor = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_addr.s_addr = INADDR_ANY;
    enderecoServidor.sin_port = htons(porta);

    bind(socketServidor, (sockaddr*)&enderecoServidor, sizeof(enderecoServidor));
    listen(socketServidor, SOMAXCONN);

    carregarHistorico();

    std::cout << "Servidor iniciado na porta " << porta << std::endl;

    while (true) {
        sockaddr_in enderecoCliente;
        int tamanho = sizeof(enderecoCliente);
        SOCKET socketCliente = accept(socketServidor, (sockaddr*)&enderecoCliente, &tamanho);
        if (socketCliente == INVALID_SOCKET) {
            continue;
        }
        std::thread(tratarCliente, socketCliente).detach();
    }

    closesocket(socketServidor);
    WSACleanup();
    return 0;
}
