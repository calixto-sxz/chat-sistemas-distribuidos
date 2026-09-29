// Aqui so importo as ferramentas prontas que uso: uma pra mexer em arquivo, uma pra criar as "threads" (que vou explicar ja ja), e uma pra fazer a conexao de rede no Windows.
#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Crio duas listas vazias que o programa inteiro vai usar: uma com quem esta conectado agora, e outra com todas as mensagens ja mandadas.
std::vector<SOCKET> clientes;
std::mutex clientesMutex;
std::vector<std::string> historico;
std::mutex historicoMutex;
const std::string arquivoHistorico = "historico.txt";

// Essa funcao le o arquivo de historico salvo no computador e coloca cada linha dele dentro da lista de mensagens - e rodada uma vez, quando o servidor liga.
void carregarHistorico() {
    std::ifstream arquivo(arquivoHistorico);
    std::string linha;
    while (std::getline(arquivo, linha)) {
        historico.push_back(linha);
    }
}

// Essa funcao escreve uma mensagem nova no final do arquivo, pra nao perder quando desligar.
void salvarNoArquivo(const std::string& mensagem) {
    std::ofstream arquivo(arquivoHistorico, std::ios::app);
    arquivo << mensagem << "\n";
}

// Essa funcao manda um texto pra UMA pessoa especifica.
void enviarMensagem(SOCKET socket, const std::string& mensagem) {
    std::string msg = mensagem + "\n";
    send(socket, msg.c_str(), (int)msg.size(), 0);
}

// Essa funcao pega esse "mandar pra uma pessoa" e repete pra TODO MUNDO que esta conectado - e como as mensagens chegam pro grupo inteiro.
void broadcast(const std::string& mensagem) {
    std::lock_guard<std::mutex> lock(clientesMutex);
    for (SOCKET cliente : clientes) {
        enviarMensagem(cliente, mensagem);
    }
}

// Essa funcao tira uma pessoa da lista de conectados, quando ela sai do chat.
void removerCliente(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientesMutex);
    for (size_t i = 0; i < clientes.size(); i++) {
        if (clientes[i] == socket) {
            clientes.erase(clientes.begin() + i);
            break;
        }
    }
}

// Essa e a funcao principal - ela roda uma vez para CADA pessoa que entra no chat.
void tratarCliente(SOCKET socket) {
    char buffer[1024];

    // Primeira coisa que essa funcao faz: recebe o nome que a pessoa digitou.
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

    // Manda pra essa pessoa todo o historico de mensagens antigas, uma por uma.
    {
        std::lock_guard<std::mutex> lock(historicoMutex);
        for (const std::string& linha : historico) {
            enviarMensagem(socket, linha);
        }
    }

    // Coloca essa pessoa na lista de quem esta conectado.
    {
        std::lock_guard<std::mutex> lock(clientesMutex);
        clientes.push_back(socket);
    }

    // Avisa todo mundo que "fulano entrou no chat" e guarda esse aviso no historico tambem.
    std::string mensagemEntrada = nomeUsuario + " entrou no chat";
    {
        std::lock_guard<std::mutex> lock(historicoMutex);
        historico.push_back(mensagemEntrada);
        salvarNoArquivo(mensagemEntrada);
    }
    broadcast(mensagemEntrada);
    std::cout << mensagemEntrada << std::endl;

    // Aqui e onde ela fica esperando a pessoa mandar mensagem. Toda vez que chega uma mensagem nova: guarda na lista, salva no arquivo, e manda pra todo mundo.
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

    // Quando a pessoa desconecta, avisa "fulano saiu", tira ela da lista, e encerra a conexao dela.
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

// Essa e a parte que roda quando o programa liga.
int main(int argc, char* argv[]) {
    int porta = 5000;
    if (argc > 1) {
        porta = std::stoi(argv[1]);
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Cria a conexao de rede.
    SOCKET socketServidor = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_addr.s_addr = INADDR_ANY;
    enderecoServidor.sin_port = htons(porta);

    // Define qual "porta" o servidor vai usar - pensa nisso como o numero que os clientes vao discar pra achar o servidor.
    bind(socketServidor, (sockaddr*)&enderecoServidor, sizeof(enderecoServidor));
    // Deixa o servidor pronto pra aceitar gente entrando.
    listen(socketServidor, SOMAXCONN);

    // Carrega o historico salvo (chama aquela funcao la de cima).
    carregarHistorico();

    std::cout << "Servidor iniciado na porta " << porta << std::endl;

    // Aqui e o coracao: o servidor fica parado esperando alguem conectar. Quando conecta, ele manda essa pessoa ser atendida separadamente e ja volta a esperar a proxima - e assim que varias pessoas conseguem entrar ao mesmo tempo sem travar.
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
