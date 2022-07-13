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
#include <string>
#include <unistd.h>
#include <arpa/inet.h>

#include "server.h"

/*
 * Local Macro
 */

/*
 * Local Constants
 */
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
    signal_received.store(false);
    shutdown_requested.store(false);
    client_status_variable = false;

    signal_thread = nullptr;
    socket_thread = nullptr;
    reply_thread = nullptr;
    client_status_thread = nullptr;
    seeder_server_port = seeder_server_default_port;

    seeder_server_socket = -1;
}

SeederServer::~SeederServer()
{
    if (seeder_server_socket > 0)
    {
        close(seeder_server_socket);
    }

    if (client_status_thread)
    {
        client_status_thread->join();
    }

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

void SeederServer::ReceiveSignalThreadFunction()
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

    signal_received.store(true);

    {
        struct sockaddr_in dummy_addr;
        size_t dummy_addr_len = sizeof(dummy_addr);
        std::string dummy_buffer(shutting_down_msg);
        std::lock_guard<std::mutex> lock(receive_queue_mutex);
        // Push an element with shutdown msg to inform ProcessReplyFunction to exit from loop
        receive_queue.emplace(dummy_buffer, dummy_buffer.size(), dummy_addr, dummy_addr_len);
        receive_queue_cv.notify_all();
    }

    {
        std::unique_lock<std::mutex> lock(client_status_check_mutex);
        client_status_variable = true;
        client_status_check_cv.notify_all();
    }

    promise_main_thread.set_value();

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

    seeder_server_address.sin_family = AF_INET;
    seeder_server_address.sin_addr.s_addr = INADDR_ANY;
    seeder_server_address.sin_port = htons(seeder_server_port);

    if (bind(seeder_server_socket, reinterpret_cast<sockaddr *>(&seeder_server_address), sizeof(seeder_server_address)) != 0)
    {
        ERROR_PRINT_LN("Bind failed for server socket");
        return status_error;
    }

    socket_thread.reset(new std::thread(&SeederServer::SocketThreadFunction, this));

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::SocketThreadFunction()
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

                // Emplace to queue and notify
                {
                    std::lock_guard<std::mutex> lock(receive_queue_mutex);
                    receive_queue.emplace(buffer, num_bytes_received, client_addr, client_addr_len);
                    receive_queue_cv.notify_all();
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
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::CheckAndAddToTable(struct sockaddr_in client_address,
        size_t client_address_len)
{
    int ret;
    char *error_msg;
    bool match_found;
    DEBUG_PRINT_LN("");

    match_found = false;
    std::lock_guard<std::mutex> lock(client_info_list_mutex);
    for (auto client_info : client_info_list)
    {
        if (client_info.client_address.sin_addr.s_addr == client_address.sin_addr.s_addr
                && client_info.client_address.sin_port == client_address.sin_port)
        {
            match_found = true;
            break;
        }
    }

    if (!match_found)
    {
        client_info_list.emplace_back(client_address, client_address_len);
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::PrepareNodesList(struct sockaddr_in client_address,
        size_t client_address_len)
{
    std::string buffer;
    char ip_address[INET_ADDRSTRLEN];

    DEBUG_PRINT_LN("");

    {
        buffer.append(get_nodes_list_msg);
        buffer.append("*");
        std::lock_guard<std::mutex> lock(client_info_list_mutex);
        for(auto client_info : client_info_list)
        {
            std::memset(ip_address, 0, sizeof(ip_address));
            if (inet_ntop(AF_INET, &client_info.client_address.sin_addr, ip_address, INET_ADDRSTRLEN) == 0x0)
            {
                return status_error;
            }

            buffer.append(ip_address);
            buffer.append(":");
            buffer.append(std::to_string(client_info.client_address.sin_port));
            buffer.append("*");
        }
    }

    sendto(seeder_server_socket, (char *)buffer.c_str(), buffer.size(), MSG_WAITALL,
            (struct sockaddr *) &client_address, client_address_len);

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::PingReceived(struct sockaddr_in client_address,
        size_t client_address_len)
{
    DEBUG_PRINT_LN("");

    {
        std::lock_guard<std::mutex> lock(client_info_list_mutex);
        for(auto &client_info : client_info_list)
        {
            if (client_info.client_address.sin_addr.s_addr == client_address.sin_addr.s_addr
                            && client_info.client_address.sin_port == client_address.sin_port)
            {
                client_info.last_ping_received_time = std::chrono::steady_clock::now();
            }
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::PrepareDurationAliveList(struct sockaddr_in client_address,
        size_t client_address_len, int time_alive)
{
    std::string buffer;
    char ip_address[INET_ADDRSTRLEN];

    DEBUG_PRINT_LN("");

    {
        buffer.append(duration_alive_msg);
        buffer.append("*");
        std::lock_guard<std::mutex> lock(client_info_list_mutex);
        for(auto client_info : client_info_list)
        {
            std::chrono::duration<double> time_span = std::chrono::duration_cast<
                                std::chrono::duration<double>>(std::chrono::steady_clock::now() - client_info.joining_time);
            DEBUG_PRINT_LN("Client ", client_info.client_address.sin_addr.s_addr, ":", client_info.client_address.sin_port,
                    "is alive for ", time_span.count(), " seconds");

            std::memset(ip_address, 0, sizeof(ip_address));
            if (inet_ntop(AF_INET, &client_info.client_address.sin_addr, ip_address, INET_ADDRSTRLEN) == 0x0)
            {
                return status_error;
            }

            if (time_span.count() > time_alive)
            {
                buffer.append(ip_address);
                buffer.append(":");
                buffer.append(std::to_string(client_info.client_address.sin_port));
                buffer.append("*");
            }
        }
    }

    sendto(seeder_server_socket, (char *)buffer.c_str(), buffer.size(), MSG_WAITALL,
            (struct sockaddr *) &client_address, client_address_len);

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::ProcessReplyThreadFunction()
{
    receive_queue_data rqd;

    DEBUG_PRINT_LN("");

    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(receive_queue_mutex);
            receive_queue_cv.wait(lock, [this]()
                {
                    return !receive_queue.empty();
                });
            if (!receive_queue.empty())
            {
                rqd = receive_queue.front();
                receive_queue.pop();
            }
        }
        DEBUG_PRINT_LN("", rqd.buffer, " ", rqd.client_address.sin_port);
        if (rqd.buffer == hello_msg)
        {
            // Client introduction, add to list if not present previously
            CheckAndAddToTable(rqd.client_address, rqd.client_addr_len);
        }
        else if (rqd.buffer == get_nodes_list_msg)
        {
            // Client asking for nodes list, prepare and send.
            PrepareNodesList(rqd.client_address, rqd.client_addr_len);
        }
        else if(rqd.buffer == ping_msg)
        {
            // Update last ping received time in client info list
            PingReceived(rqd.client_address, rqd.client_addr_len);
        }
        else if (rqd.buffer.find(duration_alive_msg) != std::string::npos)
        {
            int time_alive = std::stoi(rqd.buffer.substr(duration_alive_msg.size() + 1), nullptr, 10);
            PrepareDurationAliveList(rqd.client_address, rqd.client_addr_len, time_alive);
        }
        else if (rqd.buffer == shutting_down_msg)
        {
            break;
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::ClientStatusThreadFunction()
{
    DEBUG_PRINT_LN("");

    while(!signal_received.load())
    {
        {
            std::unique_lock<std::mutex> lock(client_status_check_mutex);
            client_status_check_cv.wait_for(lock, std::chrono::seconds(2), [this]()
                    {
                        return client_status_variable;
                    });

            if (client_status_variable)
            {
                break;
            }
        }

        std::lock_guard<std::mutex> lock1(client_info_list_mutex);
        for(auto iterator = client_info_list.begin(); iterator != client_info_list.end(); ++iterator)
        {
            std::chrono::duration<double> time_span = std::chrono::duration_cast<
                    std::chrono::duration<double>>(std::chrono::steady_clock::now() - iterator->last_ping_received_time);
            DEBUG_PRINT_LN("", iterator->client_address.sin_port, " ", time_span.count());
            if (time_span.count() > 11)
            {
                iterator = client_info_list.erase(iterator);
            }
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e SeederServer::StartThreads()
{
    signal_thread.reset(new std::thread(&SeederServer::ReceiveSignalThreadFunction, this));

    if (SetupSocket() != status_ok)
    {
        shutdown_requested.store(true);
        return status_error;
    }

    reply_thread.reset(new std::thread(&SeederServer::ProcessReplyThreadFunction, this));

    client_status_thread.reset(new std::thread(&SeederServer::ClientStatusThreadFunction, this));

    INFO_PRINT_LN("Server running");

    auto future = promise_main_thread.get_future();
    future.wait();

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

    INFO_PRINT_LN("Server shutting down");
    return;
}

int main()
{
    server();

    return 0;
}
