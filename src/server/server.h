/*
 * server.h
 *
 *  Created on: 9 Jul 2022
 *  Author: Vasanth Ragavendran
 */
#ifndef SERVER_H_
#define SERVER_H_

#include <thread>
#include <memory>
#include <atomic>

#include <signal.h>
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
class SeederServer
{
public:
    SeederServer();
    ~SeederServer();

    status_e BlockSignals();
    void ReceiveSignal();
    status_e SetupSocket();
    status_e SocketFunction();
    status_e StartThreads();

private:
    std::unique_ptr<std::thread> signal_thread;
    std::unique_ptr<std::thread> socket_thread;
//    std::unique_ptr<std::thread> reply_thread;
    sigset_t sigset;
    std::atomic<bool> shutdown_requested;

    int seeder_server_socket;
    struct sockaddr_in address;
    int seeder_server_port;
};

#endif // SERVER_H_
