#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using std::cout, std::cin, std::endl;

//CONFIG
const int MAX_EVENTS = 128;
const int PORT = 8080;

int handle_client(int client_fd, sockaddr_in client_addr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    cout << "Connected to client: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

    char buffer[1024] = {0};
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes < 0) {
        perror("recv");
        close(client_fd);
        return 1;
    }

    cout << "Received: " << buffer << endl;

    send(client_fd, buffer, bytes, 0);

    close(client_fd);

    return 0;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        close(server_fd);
        return 1;
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    struct epoll_event event, events[MAX_EVENTS];
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll");
        close(server_fd);
        return 1;
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll");
        close(server_fd);
        close(epoll_fd);
    }

    cout << "Waiting for client...\n";

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            std::cerr << "Failed to wait for events." << std::endl;
            break;
        }

        for (int i = 0; i < num_events; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);

                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    perror("epoll socket addition");
                    close(client_fd);
                }
            } else {
                sockaddr_in dummy_addr;
                std::thread(handle_client, fd, dummy_addr).detach();
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            }
        }
    }

    close(server_fd);
    close(epoll_fd);

    return 0;
}
