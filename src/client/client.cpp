/*
 * client.cpp
 *
 *  Created on: 9 Jul 2022
 *  Author: Vasanth Ragavendran
 */
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <future>
#include <sstream>

#include <unistd.h>
#include <arpa/inet.h>

#include "client.h"

/*
 * Local Macro
 */

/*
 * Local Constants
 */
static const long select_wait_seconds = 0;
static const long select_wait_micro_seconds = 100000; // 100 milliseconds
static const int client_receive_buffer_size = 4096;
const std::string command_prompt("\nPlease specify the option below:\n" \
        "1. Send hello to server\n" \
        "2. Get peer list\n" \
        "3. Nodes which are alive during the last x secs/mins/hrs/days etc\n" \
        "4. Msg peer nodes\n" \
        "5. Quit/Shutdown client");

/*
 * Globals
 */
Client::Client()
{
    shutdown_requested.store(false);

    socket_thread = nullptr;
    process_input_thread = nullptr;
    ping_thread = nullptr;
    reply_thread = nullptr;
    ping_mutex_variable = false;

    hello_sent.store(false);

    client_socket = -1;
}

Client::~Client()
{
    if (!hello_sent.load())
    {
        promise_hello_sent.set_value();
    }

    if (reply_thread)
    {
        reply_thread->join();
    }

    if (ping_thread)
    {
        ping_thread->join();
    }

    if (process_input_thread)
    {
        process_input_thread->join();
    }

    if (socket_thread)
    {
        socket_thread->join();
    }

    if (client_socket)
    {
        close(client_socket);
    }
}

status_e Client::SetupSocket(int argc, char **argv)
{
    struct sockaddr_in client_address;

    DEBUG_PRINT_LN("");

    if ((client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == 0)
    {
        ERROR_PRINT_LN("Seeder server socket creation failed");
        return status_error;
    }

    int opt = 1;
    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
            &opt, sizeof(opt)) != 0)
    {
        ERROR_PRINT_LN("Setsockopt error");
        return status_error;
    }

    // Filling server information
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(seeder_server_default_port);
    if (argc > 1)
    {
        if (inet_pton(AF_INET, argv[1], &server_address.sin_addr) <= 0)
        {
            ERROR_PRINT_LN("inet_pton error");
            return status_error;
        }
    }
    else
    {
        server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    server_address_len = sizeof(server_address);

    client_address.sin_family = AF_INET;
    client_address.sin_port = 0;
    client_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ( bind(client_socket, reinterpret_cast<sockaddr *>(&client_address),
                    sizeof(client_address)) < 0 )
    {
        ERROR_PRINT_LN("bind error");
        return status_error;
    }

    socket_thread.reset(new std::thread(&Client::SocketThreadFunction, this));

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::SocketThreadFunction()
{
    fd_set readfds;
    struct timeval tv;
    int nfds;
    int result;
    char buffer[client_receive_buffer_size];
    size_t num_bytes_received;
    struct sockaddr_in addr;
    socklen_t addr_len;
    char ip_address[INET_ADDRSTRLEN];

    DEBUG_PRINT_LN("");
    nfds = 0;

    addr_len = sizeof(addr);
    while(!shutdown_requested.load())
    {
        FD_ZERO(&readfds);
        FD_SET(client_socket, &readfds);
        std::memset(buffer, 0, client_receive_buffer_size);

        nfds = client_socket + 1;

        tv.tv_sec = select_wait_seconds;
        tv.tv_usec = select_wait_micro_seconds;

        result = select(nfds, &readfds, (fd_set *)nullptr, (fd_set *)nullptr, &tv);

        if (result > 0)
        {
            if (FD_ISSET(client_socket, &readfds))
            {
                if ((num_bytes_received = recvfrom(client_socket, (char *)buffer, client_receive_buffer_size,
                                               MSG_WAITALL, (struct sockaddr *) &addr,
                                               &addr_len)) > 0)
                {
                    buffer[num_bytes_received] = '\0';
                    if (buffer == hello_msg)
                    {
                        std::memset(ip_address, 0, sizeof(ip_address));
                        if (inet_ntop(AF_INET, &addr.sin_addr, ip_address, INET_ADDRSTRLEN) == 0x0)
                        {
                            return status_error;
                        }
                        INFO_PRINT_LN("", buffer, " from ", ip_address, ":", ntohs(addr.sin_port));
                    }

                    // Push the buffer elements to queue
                    receive_queue_data rqd(buffer, num_bytes_received);
                    receive_queue.Push(rqd);
                }
            }
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::ProcessInputThreadFunction()
{
    std::string input;

    DEBUG_PRINT_LN("");

    while(!shutdown_requested.load())
    {
        INFO_PRINT_LN("", command_prompt);
        std::cin >> input;

        if (input == "1")
        {
            if (sendto(client_socket, (void *)hello_msg.data(), hello_msg.size(),
                    MSG_WAITALL, (struct sockaddr *) &server_address, server_address_len) < 0)
            {
                ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
            }

            if (!hello_sent.load())
            {
                promise_hello_sent.set_value();
                hello_sent.store(true);
            }
        }
        else if (input == "2" && hello_sent.load())
        {
            if (sendto(client_socket, (void *)get_nodes_list_msg.data(), get_nodes_list_msg.size(),
                                MSG_WAITALL, (struct sockaddr *) &server_address, server_address_len) < 0)
            {
                ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
                continue;
            }
            std::this_thread::sleep_for (std::chrono::seconds(1));
        }
        else if (input == "3" && hello_sent.load())
        {
            INFO_PRINT_LN("Specify time period like 20s, 10m, 2h, 1d:");
            std::string duration_alive;
            size_t find;
            std::cin >> input;
            int time_in_seconds;

            if ((find = input.find("s")) != std::string::npos ||
                    (find = input.find("m")) != std::string::npos ||
                    (find = input.find("h")) != std::string::npos ||
                    (find = input.find("d")) != std::string::npos)
            {
                if (input[find] == 's')
                {
                    time_in_seconds = std::stoi(input.substr(0, find), nullptr, 10);
                }
                else if (input[find] == 'm')
                {
                    time_in_seconds = std::stoi(input.substr(0, find), nullptr, 10) * 60;
                }
                else if (input[find] == 'h')
                {
                    time_in_seconds = std::stoi(input.substr(0, find), nullptr, 10) * 60 * 60;
                }
                if (input[find] == 'd')
                {
                    time_in_seconds = std::stoi(input.substr(0, find), nullptr, 10) * 60 * 60 * 24;
                }

                duration_alive = duration_alive_msg + "*" + std::to_string(time_in_seconds);

                if (sendto(client_socket, (void *)duration_alive.data(), duration_alive.size(),
                                                MSG_WAITALL, (struct sockaddr *) &server_address, server_address_len) < 0)
                {
                    ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
                }
            }
            else
            {
                ERROR_PRINT_LN("Invalid time input, please specify again");
            }
            std::this_thread::sleep_for (std::chrono::seconds(1));
        }
        else if (input == "4")
        {
            INFO_PRINT_LN("Specify peer's IP address and port (eg. 127.0.0.1:47851):");
            std::cin >> input;
            size_t find;
            std::string ip_address;
            uint port;
            struct sockaddr_in peer_address;
            size_t peer_address_len;
            std::stringstream ss;

            if ((find = input.find(":")) != std::string::npos)
            {
                ip_address = input.substr(0, find);
                ss << input.substr(find + 1);
                ss >> port;

                if (inet_pton(AF_INET, ip_address.c_str(), &(peer_address.sin_addr)) != 1 || port < 0
                     || port > 65535)
                {
                    ERROR_PRINT_LN("IP address/port format wrong, please specify again");
                    continue;
                }

                peer_address.sin_family = AF_INET;
                peer_address.sin_port = htons(port);
//                peer_address.sin_addr.s_addr = inet_addr(ip_address.c_str());
                peer_address_len = sizeof(peer_address);
//                INFO_PRINT_LN("IP address ", ip_address, " port ", port, " ", peer_address.sin_addr.s_addr);

                if (sendto(client_socket, (void *)hello_msg.data(), hello_msg.size(),
                        MSG_WAITALL, (struct sockaddr *) &peer_address, peer_address_len) < 0)
                {
                    ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
                }

                {
                    std::lock_guard<std::mutex> lock(peer_info_list_mutex);
                    bool match_found = false;
                    for (auto peer_info : peer_info_list)
                    {
                        if ((peer_info.peer_ip_address.compare(ip_address) == 0)
                             && (peer_info.peer_port == (uint16_t)port))
                        {
                            match_found = true;
                            break;
                        }
                    }

                    if (!match_found)
                    {
                        peer_info_list.emplace_back(ip_address, port);
                    }
                }
            }
            else
            {
                ERROR_PRINT_LN("Invalid IP address/port input, please specify again");
            }
        }
        else if (input == "5")
        {
            shutdown_requested.store(true);

            promise_main_thread.set_value();

            {
                std::unique_lock<std::mutex> lock(ping_mutex);
                ping_mutex_variable = true;
                ping_cv.notify_all();
            }

            {
                // Push an element with shutdown msg to inform ProcessReplyFunction to exit from loop
                receive_queue_data rqd(shutting_down_msg, shutting_down_msg.size());
                receive_queue.Push(rqd);
            }
        }
        else
        {
            ERROR_PRINT_LN("Input error or send hello before inputting other choice. Choose again between [1-5].");
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::PingServerThreadFunction()
{
    int first_time;
    std::string peer_info_list_msg;
    DEBUG_PRINT_LN("");

    first_time = 0;
    auto fut = promise_hello_sent.get_future();

    while(true)
    {
        if (!first_time)
        {
            // Wait for the hello msg to be sent to server before starting to ping
            // server periodically.
            fut.wait();
            first_time = 1;
        }
        {
            std::unique_lock<std::mutex> lock(ping_mutex);
            ping_cv.wait_for(lock, std::chrono::seconds(10), [this]()
                    {
                        return ping_mutex_variable;
                    });

            if(ping_mutex_variable)
            {
                break;
            }
        }

        DEBUG_PRINT_LN("Sending ping now");
        if (sendto(client_socket, (void *)ping_msg.data(), ping_msg.size(),
                            MSG_WAITALL, (struct sockaddr *) &server_address, server_address_len) < 0)
        {
            ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
        }

        DEBUG_PRINT_LN("Also send the list of clients/nodes I'm msging to");
        peer_info_list_msg = peer_info_msg + "*";
        {
            std::lock_guard<std::mutex> lock(peer_info_list_mutex);
            for (auto peer_info : peer_info_list)
            {
                peer_info_list_msg += peer_info.peer_ip_address + ":" + std::to_string(peer_info.peer_port) + "*";
            }
        }

//        INFO_PRINT_LN("", peer_info_list_msg);
        if (sendto(client_socket, (void *)peer_info_list_msg.data(), peer_info_list_msg.size(),
                            MSG_WAITALL, (struct sockaddr *) &server_address, server_address_len) < 0)
        {
            ERROR_PRINT_LN("Sendto returned error: ", strerror(errno), "(", errno, ")");
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::ProcessReplyThreadFunction()
{
    receive_queue_data rqd;
    size_t position1;
    size_t position2;

    DEBUG_PRINT_LN("");

    while(true)
    {
        // Pop function will wait for an element to be pushed to queue and returns
        // the front element from the queue.
        receive_queue.Pop(rqd);

        if ((position1 = rqd.buffer.find(get_nodes_list_msg)) != std::string::npos)
        {
            INFO_PRINT_LN("IP addresses:Ports*peer_info_list");
            position1 = get_nodes_list_msg.size() + 1;
            position2 = rqd.buffer.find("%", position1);
            while(position2 != std::string::npos)
            {
                INFO_PRINT_LN("", rqd.buffer.substr(position1, position2 - position1));
                position1 = position2 + 1;
                position2 = rqd.buffer.find("%", position1);
            }
        }
        else if ((position1 = rqd.buffer.find(duration_alive_msg)) != std::string::npos)
        {
            INFO_PRINT_LN("IP addresses:Ports");
            position1 = duration_alive_msg.size() + 1;
            position2 = rqd.buffer.find("*", position1);
            while(position2 != std::string::npos)
            {
                INFO_PRINT_LN("", rqd.buffer.substr(position1, position2 - position1));
                position1 = position2 + 1;
                position2 = rqd.buffer.find("*", position1);
            }
        }
        else if (rqd.buffer == shutting_down_msg)
        {
            break;
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}
status_e Client::StartThreads(int argc, char **argv)
{
    if (SetupSocket(argc, argv) != status_ok)
    {
        shutdown_requested.store(true);
        return status_error;
    }

    process_input_thread.reset(new std::thread(&Client::ProcessInputThreadFunction, this));

    ping_thread.reset(new std::thread(&Client::PingServerThreadFunction, this));

    reply_thread.reset(new std::thread(&Client::ProcessReplyThreadFunction, this));

    auto future = promise_main_thread.get_future();
    future.wait();

    return status_ok;
}

void client(int argc, char **argv)
{
    Client c;

    if(c.StartThreads(argc, argv) != status_ok)
    {
        ERROR_PRINT_LN("Start Threads failed");
        return;
    }

    INFO_PRINT_LN("Client shutting down");

    return;
}

int main(int argc, char **argv)
{
    client(argc, argv);

    return 0;
}
