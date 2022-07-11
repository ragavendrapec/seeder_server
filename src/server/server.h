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
#include <queue>

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
struct receive_socket_data
{
    receive_socket_data() {}

    receive_socket_data(std::string arg_buffer, size_t arg_buffer_size, struct sockaddr_in arg_client_address,
            size_t arg_client_addr_len): buffer(arg_buffer), buffer_size(arg_buffer_size),
                    client_address(arg_client_address), client_addr_len(arg_client_addr_len)
    {}

    std::string buffer;
    size_t buffer_size;
    struct sockaddr_in client_address;
    size_t client_addr_len;
} ;

class SeederServer
{
public:
    SeederServer();
    ~SeederServer();

    status_e BlockSignals();
    void ReceiveSignal();
    status_e SetupSocket();
    status_e SocketFunction();
    status_e ProcessReply();
    status_e StartThreads();

private:
    std::unique_ptr<std::thread> signal_thread;
    std::unique_ptr<std::thread> socket_thread;
    std::unique_ptr<std::thread> reply_thread;

    sigset_t sigset;
    std::atomic<bool> shutdown_requested;

    int seeder_server_socket;
    struct sockaddr_in address;
    int seeder_server_port;

    std::mutex queue_signal_mutex;
    std::queue<receive_socket_data> receive_socket_queue;
    std::condition_variable queue_signal_cv;
    bool signal_received;
};

#endif // SERVER_H_
