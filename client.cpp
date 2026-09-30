// Aqui so importo as ferramentas prontas que uso: uma pra criar a "thread" (linha separada de execucao), e uma pra fazer a conexao de rede no Windows.
#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Essa funcao fica so recebendo mensagem do servidor e mostrando na tela. Ela roda numa thread separada, enquanto a tela principal cuida de deixar a pessoa digitar.
void receberMensagens(SOCKET socket) {
    char buffer[1024];
    while (true) {
        // Fica parado aqui esperando o servidor mandar alguma coisa.
        int bytesRecebidos = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRecebidos <= 0) {
            std::cout << "Conexao com o servidor encerrada" << std::endl;
            break;
        }
        buffer[bytesRecebidos] = '\0';
        std::cout << buffer;
    }
}

// Essa e a parte que roda quando o programa liga.
int main(int argc, char* argv[]) {
    // Define pra qual computador (IP) e qual porta vai conectar. Se a pessoa passar valores na hora de abrir o programa, usa esses valores no lugar do padrao (127.0.0.1 e 5000).
    std::string ip = "127.0.0.1";
    int porta = 5000;
    if (argc > 1) {
        ip = argv[1];
    }
    if (argc > 2) {
        porta = std::stoi(argv[2]);
    }

    // Prepara a biblioteca de rede do Windows, obrigatorio antes de usar socket.
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Cria a conexao de rede.
    SOCKET socketCliente = socket(AF_INET, SOCK_STREAM, 0);

    // Monta o endereco do servidor: junta o IP com a porta.
    sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(porta);
    inet_pton(AF_INET, ip.c_str(), &enderecoServidor.sin_addr);

    // Tenta conectar no servidor usando esse endereco. Se nao conseguir, avisa e encerra o programa.
    if (connect(socketCliente, (sockaddr*)&enderecoServidor, sizeof(enderecoServidor)) != 0) {
        std::cout << "Nao foi possivel conectar ao servidor" << std::endl;
        WSACleanup();
        return 1;
    }

    // Pede o nome de usuario pra pessoa digitar, e manda esse nome pro servidor.
    std::string nomeUsuario;
    std::cout << "Digite seu nome de usuario: ";
    std::getline(std::cin, nomeUsuario);

    std::string mensagemNome = nomeUsuario + "\n";
    send(socketCliente, mensagemNome.c_str(), (int)mensagemNome.size(), 0);

    // Cria uma linha de execucao separada (thread) so pra ficar recebendo mensagem do servidor, rodando junto com o resto do programa.
    std::thread threadRecebimento(receberMensagens, socketCliente);
    threadRecebimento.detach();

    std::cout << "Conectado ao chat. Digite 'sair' para encerrar." << std::endl;

    // Fica lendo o que a pessoa digita no teclado e manda pro servidor. Se a pessoa digitar "sair", para de esperar e encerra.
    std::string texto;
    while (true) {
        std::getline(std::cin, texto);
        if (texto == "sair") {
            break;
        }
        std::string mensagem = texto + "\n";
        send(socketCliente, mensagem.c_str(), (int)mensagem.size(), 0);
    }

    // Fecha a conexao quando o programa termina.
    closesocket(socketCliente);
    WSACleanup();
    return 0;
}
