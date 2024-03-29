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
#include <list>
#include <chrono>
#include <future>

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
struct client_info
{
    client_info() {}

    client_info(struct sockaddr_in arg_client_address, size_t arg_client_addr_len)
                    : client_addr_len(arg_client_addr_len), joining_time(std::chrono::steady_clock::now()),
                      last_ping_received_time(std::chrono::steady_clock::now())
    {
        memcpy(&client_address, &arg_client_address, arg_client_addr_len);
    }

    struct sockaddr_in client_address;
    size_t client_addr_len;
    std::chrono::steady_clock::time_point joining_time;
    std::chrono::steady_clock::time_point last_ping_received_time;
    std::string peer_info_list;
};

class SeederServer
{
public:
    SeederServer();
    ~SeederServer();

    status_e BlockSignals();
    void ReceiveSignalThreadFunction();
    status_e SetupSocket();
    status_e SocketThreadFunction();
    status_e CheckAndAddToTable(struct sockaddr_in client_address, size_t client_address_len);
    status_e PrepareNodesList(struct sockaddr_in client_address, size_t client_address_len);
    status_e PingReceived(struct sockaddr_in client_address, size_t client_address_len);
    status_e PrepareDurationAliveList(struct sockaddr_in client_address,size_t client_address_len,
            int time_alive);
    status_e PeerInfoListReceived(struct sockaddr_in client_address,size_t client_address_len,
            std::string peer_info_list);
    status_e ProcessReplyThreadFunction();
    status_e ClientStatusThreadFunction();
    status_e StartThreads();
    std::list<client_info>::iterator FindClientInfo(struct sockaddr_in client_address);

private:
    std::unique_ptr<std::thread> signal_thread;
    std::unique_ptr<std::thread> socket_thread;
    std::unique_ptr<std::thread> reply_thread;
    std::unique_ptr<std::thread> client_status_thread;

    sigset_t sigset;
    std::atomic<bool> shutdown_requested;

    int seeder_server_socket;
    struct sockaddr_in seeder_server_address;
    int seeder_server_port;

    Queue<receive_queue_data> receive_queue;

    std::atomic<bool> signal_received;

    std::mutex client_info_list_mutex;
    std::list<client_info> client_info_list;

    std::mutex client_status_check_mutex;
    std::condition_variable client_status_check_cv;
    bool client_status_variable;

    std::promise<void> promise_main_thread;
};

#endif // SERVER_H_
