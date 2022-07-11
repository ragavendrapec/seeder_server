/*
 * server.cpp
 *
 *  Created on: 9 Jul 2022
 *  Author: Vasanth Ragavendran
 */
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <cstring>

#include "server.h"

/*
 * Local Macro
 */

/*
 * Local Constants
 */
static const int seeder_server_default_port = 54321;
static const long seeder_server_select_wait_seconds = 0;
static const long seeder_server_select_wait_micro_seconds = 100000; // 100 milliseconds
static const int seeder_server_receive_buffer_size = 4096;

/*
 * Local Types
 */

/*
 * Globals
 */
SeederServer::SeederServer()
{
    signal_received = false;
    shutdown_requested.store(false);

    signal_thread = nullptr;
    socket_thread = nullptr;
    reply_thread = nullptr;
    seeder_server_port = seeder_server_default_port;
}

SeederServer::~SeederServer()
{
    if (reply_thread)
    {
        reply_thread->join();
    }

    if (socket_thread)
    {
        socket_thread->join();
    }

    if (signal_thread)
    {
        signal_thread->join();
    }
}

status_e SeederServer::BlockSignals()
{
    int ret;
    DEBUG_PRINT_LN("");

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGQUIT);

    ret = pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    if (ret != 0)
    {
        ERROR_PRINT_LN("sigmask failed");
        return status_error;
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

void SeederServer::ReceiveSignal()
{
    int signal;
    struct timespec t;

    DEBUG_PRINT_LN("");

    t.tv_sec = 0;
    t.tv_nsec = 100000000; // 100 millisec

    while(!shutdown_requested.load())
    {
        signal = sigtimedwait(&sigset, nullptr, &t);
        if ((signal == -1) && (errno == EAGAIN))
        {
            // Timeout occurred, so continue
            continue;
        }
        else
        {
            INFO_PRINT_LN("Signal received ", signal);
            break;
        }
    }

    std::lock_guard<std::mutex> lock(queue_signal_mutex);
    signal_received = true;
    queue_signal_cv.notify_all();

    DEBUG_PRINT_LN("completed");
    return;
}

status_e SeederServer::SetupSocket()
{
    DEBUG_PRINT_LN("");

    if ((seeder_server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        ERROR_PRINT_LN("Seeder server socket creation failed");
        return status_error;
    }

    int opt = 1;
    if (setsockopt(seeder_server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
            &opt, sizeof(opt)) != 0)
    {
        ERROR_PRINT_LN("Setsockopt error");
        return status_error;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(seeder_server_port);

    if (bind(seeder_server_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
    {
        ERROR_PRINT_LN("Bind failed for server socket");
        return status_error;
    }

    socket_thread.reset(new std::thread(&SeederServer::SocketFunction, this));

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::SocketFunction()
{
    fd_set readfds;
    struct timeval tv;
    int nfds;
    int result;
    char buffer[seeder_server_receive_buffer_size];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    size_t num_bytes_received;

    DEBUG_PRINT_LN("");

    nfds = 0;
    client_addr_len = sizeof(client_addr);
    while(true)
    {
        memset(&client_addr, 0, sizeof(client_addr));
        FD_ZERO(&readfds);
        FD_SET(seeder_server_socket, &readfds);

        nfds = seeder_server_socket + 1;

        tv.tv_sec = seeder_server_select_wait_seconds;
        tv.tv_usec = seeder_server_select_wait_micro_seconds;

        result = select(nfds, &readfds, (fd_set *)nullptr, (fd_set *)nullptr, &tv);

        if (result > 0)
        {
            if (FD_ISSET(seeder_server_socket, &readfds))
            {
                num_bytes_received = recvfrom(seeder_server_socket, (char *)buffer, seeder_server_receive_buffer_size,
                                               MSG_WAITALL, (struct sockaddr *) &client_addr,
                                               &client_addr_len);

                buffer[num_bytes_received] = '\0';
//                INFO_PRINT_LN("", buffer, client_addr.sin_port);

                // Emplace to queue (instead of push) and notify
                {
                    std::lock_guard<std::mutex> lock(queue_signal_mutex);
                    receive_socket_queue.push(receive_socket_data(buffer, num_bytes_received, client_addr, client_addr_len));
                    queue_signal_cv.notify_all();
                }
            }
        }
        else if (result == 0)
        {
            // timeout occurred
        }
        else
        {
            ERROR_PRINT_LN("select returned error: ", strerror(errno), "(", errno, ")");
        }

        std::lock_guard<std::mutex> lock(queue_signal_mutex);
        if (signal_received)
        {
            break;
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::ProcessReply()
{
    receive_socket_data rsd;

    DEBUG_PRINT_LN("");

    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(queue_signal_mutex);
            queue_signal_cv.wait(lock, [this]()
                {
                    return signal_received || !receive_socket_queue.empty();
                });
            if (!receive_socket_queue.empty())
            {
                rsd = receive_socket_queue.front();
                receive_socket_queue.pop();
            }

            if (signal_received)
            {
                break;
            }
        }
        INFO_PRINT_LN("", rsd.buffer, " ", rsd.client_address.sin_port);
        if (rsd.buffer == hello_msg)
        {
            // Client introduction, add to database if not present previously
        }
        else if (rsd.buffer == get_nodes_list_msg)
        {
            // Client asking for nodes list, prepare and send.
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::StartThreads()
{
    signal_thread.reset(new std::thread(&SeederServer::ReceiveSignal, this));

    if (SetupSocket() != status_ok)
    {
        shutdown_requested.store(true);
        return status_error;
    }

    reply_thread.reset(new std::thread(&SeederServer::ProcessReply, this));

    while(true)
    {
        std::unique_lock<std::mutex> lock(queue_signal_mutex);
        queue_signal_cv.wait(lock, [&]()
                {
                    return signal_received;
                });

        if (signal_received)
        {
            break;
        }
    }

    return status_ok;
}

void server()
{
    SeederServer seeder_server;

    if(seeder_server.BlockSignals() != status_ok)
    {
        ERROR_PRINT_LN("Block signals failed");
        return;
    }

    if(seeder_server.StartThreads() != status_ok)
    {
        ERROR_PRINT_LN("Start Threads failed");
        return;
    }

    INFO_PRINT_LN("Shutting down");
    return;
}

int main()
{
    server();

    return 0;
}
