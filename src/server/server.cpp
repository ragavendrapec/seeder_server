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
 * Globals
 */
std::mutex cv_mutex;
std::condition_variable cv;
std::atomic<bool> signal_received;

SeederServer::SeederServer()
{
    signal_received.store(false);
    shutdown_requested.store(false);

    signal_thread = nullptr;
    socket_thread = nullptr;
    seeder_server_port = seeder_server_default_port;
}

SeederServer::~SeederServer()
{
    if (signal_thread)
    {
        signal_thread->join();
    }

    if (socket_thread)
    {
        socket_thread->join();
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
        DEBUG_PRINT_LN("sigmask failed");
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
            DEBUG_PRINT_LN("Signal received ", signal);
            break;
        }
    }

    signal_received.store(true);
    cv.notify_all();

    DEBUG_PRINT_LN("completed");
    return;
}

status_e SeederServer::SetupSocket()
{
    DEBUG_PRINT_LN("");

    if ((seeder_server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        DEBUG_PRINT_LN("Seeder server socket creation failed");
        return status_error;
    }

    int opt = 1;
    if (setsockopt(seeder_server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
            &opt, sizeof(opt)) != 0)
    {
        DEBUG_PRINT_LN("Setsockopt error");
        return status_error;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(seeder_server_port);

    if (bind(seeder_server_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
    {
        DEBUG_PRINT_LN("Bind failed for server socket");
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
    while(!signal_received.load())
    {
        memset(&client_addr, 0, sizeof(client_addr));
        FD_ZERO(&readfds);
        FD_SET(seeder_server_socket, &readfds);

        nfds = seeder_server_socket + 1;

        tv.tv_sec = seeder_server_select_wait_seconds;
        tv.tv_usec = seeder_server_select_wait_micro_seconds;

        result = select(nfds, &readfds, (fd_set *)NULL, (fd_set *)NULL, &tv);

        if (result > 0)
        {
            if (FD_ISSET(seeder_server_socket, &readfds))
            {
                num_bytes_received = recvfrom(seeder_server_socket, (char *)buffer, seeder_server_receive_buffer_size,
                                               MSG_WAITALL, ( struct sockaddr *) &client_addr,
                                               &client_addr_len);

                buffer[num_bytes_received] = '\0';

                // Push to queue and notify
            }
        }
        else if (result == 0)
        {
            // timeout occurred
        }
        else
        {
            DEBUG_PRINT_LN("select returned error-", strerror(errno), "(", errno, ")");
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
    return status_ok;
}

void server()
{
    SeederServer seeder_server;

    if(seeder_server.BlockSignals() != status_ok)
    {
        DEBUG_PRINT_LN("Block signals failed");
        return;
    }

    if(seeder_server.StartThreads() != status_ok)
    {
        DEBUG_PRINT_LN("Start Threads failed");
        return;
    }

    while(!signal_received.load())
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv.wait(lock);
    }

    DEBUG_PRINT_LN("Shutting down");
    return;
}

int main()
{
    server();
}
