#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#define SEND_FLAGS 0
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket close
#define SEND_FLAGS MSG_NOSIGNAL
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

const int PORT = 9000;

// Список задач
vector<string> tasks;

// Приветствие при подключении клиента
string onConnect() {
    return "Hello! Commands: ADD <text>, LIST, DEL <number>, QUIT";
}

// Обработка команд
string handle(const string& command) {
    istringstream in(command);
    string name;
    in >> name;

    // Добавление задачи
    if (name == "ADD") {
        string task;
        getline(in, task);

        if (!task.empty() && task[0] == ' ')
            task.erase(0, 1);

        if (task.empty())
            return "ERROR task text is missing";

        tasks.push_back(task);

        return "OK, task " + to_string(tasks.size());
    }

    // Просмотр списка задач
    if (name == "LIST") {
        if (tasks.empty())
            return "List is empty";

        string result;

        for (int i = 0; i < (int)tasks.size(); i++) {
            if (i > 0)
                result += "; ";

            result += to_string(i + 1) + ") " + tasks[i];
        }

        return result;
    }

    // Удаление задачи
    if (name == "DEL") {
        int number;

        if (!(in >> number))
            return "ERROR enter task number";

        if (number < 1 || number >(int)tasks.size())
            return "ERROR invalid task number";

        tasks.erase(tasks.begin() + number - 1);

        return "OK, task deleted";
    }

    return "ERROR unknown command: " + name;
}

bool sendLine(SOCKET s, const string& text) {
    string data = text + "\n";
    size_t sent = 0;

    while (sent < data.size()) {
        int n = send(s, data.c_str() + sent,
            (int)(data.size() - sent), SEND_FLAGS);

        if (n <= 0) return false;
        sent += n;
    }

    return true;
}

bool recvLine(SOCKET s, string& line) {
    line.clear();
    char c;

    while (true) {
        int n = recv(s, &c, 1, 0);

        if (n <= 0) return false;
        if (c == '\n') return true;
        if (c != '\r') line += c;
    }
}

int main() {
#ifdef _WIN32
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "WSAStartup failed" << endl;
        return 1;
    }

    SetConsoleOutputCP(CP_UTF8);
#endif

    srand((unsigned)time(nullptr));

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    if (listenSock == INVALID_SOCKET) {
        cout << "socket() error" << endl;
        return 1;
    }

#ifndef _WIN32
    int yes = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
        (const char*)&yes, sizeof(yes));
#endif

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "bind() error: port " << PORT << " is busy?" << endl;
        closesocket(listenSock);
        return 1;
    }

    if (listen(listenSock, 5) == SOCKET_ERROR) {
        cout << "listen() error" << endl;
        closesocket(listenSock);
        return 1;
    }

    cout << "Server is listening on port " << PORT << endl;

    while (true) {
        sockaddr_in clientAddr = {};
        socklen_t clientLen = sizeof(clientAddr);

        SOCKET client = accept(listenSock,
            (sockaddr*)&clientAddr, &clientLen);

        if (client == INVALID_SOCKET) {
            cout << "accept() error" << endl;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));

        int clientPort = ntohs(clientAddr.sin_port);

        cout << "Client connected: " << ip << ":" << clientPort << endl;

        sendLine(client, onConnect());

        string command;

        while (recvLine(client, command)) {
            cout << "  <- " << command << endl;

            if (command == "QUIT") {
                sendLine(client, "BYE");
                break;
            }

            string answer = handle(command);

            cout << "  -> " << answer << endl;

            if (!sendLine(client, answer)) break;
        }

        closesocket(client);

        cout << "Client disconnected: "
            << ip << ":" << clientPort << endl;
    }

    closesocket(listenSock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
