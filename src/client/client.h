/*
 * server.h
 *
 *  Created on: 9 Jul 2022
 *  Author: Vasanth Ragavendran
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <thread>
#include <memory>
#include <atomic>

#include <sys/socket.h>
#include <netinet/in.h>

#include "util.h"

/*
 * Macro
 */

/*
 * Constants
 */

/*
 * Type
 */

class Client
{
public:
    Client();
    ~Client();

    status_e SetupSocket();
    status_e ProcessInput();
    status_e PingServer();
    status_e StartThreads();
    status_e SocketFunction();

private:
    std::unique_ptr<std::thread> socket_thread;
    std::unique_ptr<std::thread> process_input_thread;
    std::unique_ptr<std::thread> ping_thread;

    std::atomic<bool> shutdown_requested;
    struct sockaddr_in server_address;
    socklen_t server_address_len;

    std::mutex ping_mutex;
    std::condition_variable ping_cv;
    bool ping_mutex_variable;

    std::promise<void> promise_hello_sent;
    std::atomic<bool> hello_sent;

    std::promise<void> promise_main_thread;

    int client_socket;
};

#endif // CLIENT_H_
