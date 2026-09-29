#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

void receberMensagens(SOCKET socket) {
    char buffer[1024];
    while (true) {
        int bytesRecebidos = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRecebidos <= 0) {
            std::cout << "Conexao com o servidor encerrada" << std::endl;
            break;
        }
        buffer[bytesRecebidos] = '\0';
        std::cout << buffer;
    }
}

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int porta = 5000;
    if (argc > 1) {
        ip = argv[1];
    }
    if (argc > 2) {
        porta = std::stoi(argv[2]);
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET socketCliente = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(porta);
    inet_pton(AF_INET, ip.c_str(), &enderecoServidor.sin_addr);

    if (connect(socketCliente, (sockaddr*)&enderecoServidor, sizeof(enderecoServidor)) != 0) {
        std::cout << "Nao foi possivel conectar ao servidor" << std::endl;
        WSACleanup();
        return 1;
    }

    std::string nomeUsuario;
    std::cout << "Digite seu nome de usuario: ";
    std::getline(std::cin, nomeUsuario);

    std::string mensagemNome = nomeUsuario + "\n";
    send(socketCliente, mensagemNome.c_str(), (int)mensagemNome.size(), 0);

    std::thread threadRecebimento(receberMensagens, socketCliente);
    threadRecebimento.detach();

    std::cout << "Conectado ao chat. Digite 'sair' para encerrar." << std::endl;

    std::string texto;
    while (true) {
        std::getline(std::cin, texto);
        if (texto == "sair") {
            break;
        }
        std::string mensagem = texto + "\n";
        send(socketCliente, mensagem.c_str(), (int)mensagem.size(), 0);
    }

    closesocket(socketCliente);
    WSACleanup();
    return 0;
}
