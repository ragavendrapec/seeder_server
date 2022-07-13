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
        "3. Message another peer\n" \
        "4. Nodes which are alive during the last x secs/mins/hrs/days etc\n" \
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
    ping_mutex_variable = false;

    hello_sent.store(false);
}

Client::~Client()
{
    if (!hello_sent.load())
    {
        promise_hello_sent.set_value();
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
}

status_e Client::SetupSocket()
{
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
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    server_address_len = sizeof(server_address);

    socket_thread.reset(new std::thread(&Client::SocketFunction, this));

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::SocketFunction()
{
    fd_set readfds;
    struct timeval tv;
    int nfds;
    int result;
    char buffer[client_receive_buffer_size];
    size_t num_bytes_received;

    DEBUG_PRINT_LN("");
    nfds = 0;

    while(!shutdown_requested.load())
    {
        FD_ZERO(&readfds);
        FD_SET(client_socket, &readfds);

        nfds = client_socket + 1;

        tv.tv_sec = select_wait_seconds;
        tv.tv_usec = select_wait_micro_seconds;

        result = select(nfds, &readfds, (fd_set *)nullptr, (fd_set *)nullptr, &tv);

        if (result > 0)
        {
            if (FD_ISSET(client_socket, &readfds))
            {
                num_bytes_received = recvfrom(client_socket, (char *)buffer, client_receive_buffer_size,
                                               MSG_WAITALL, nullptr, nullptr);

                buffer[num_bytes_received] = '\0';
                INFO_PRINT_LN("", buffer);
            }
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::ProcessInput()
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
            }
        }
        else if (input == "3" && hello_sent.load())
        {

        }
        else if (input == "4" && hello_sent.load())
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
        }
        else if (input == "5")
        {
            shutdown_requested.store(true);

            promise_main_thread.set_value();
            std::unique_lock<std::mutex> lock(ping_mutex);
            ping_mutex_variable = true;
            ping_cv.notify_all();
        }
        else
        {
            ERROR_PRINT_LN("Input error or send hello before inputting other choice. Choose again between [1-5].");
        }
    }

    DEBUG_PRINT_LN("completed");
    return status_ok;
}

status_e Client::PingServer()
{
    int first_time;

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
    }

    return status_ok;
}

status_e Client::StartThreads()
{
    if (SetupSocket() != status_ok)
    {
        shutdown_requested.store(true);
        return status_error;
    }

    process_input_thread.reset(new std::thread(&Client::ProcessInput, this));

    ping_thread.reset(new std::thread(&Client::PingServer, this));

    auto future = promise_main_thread.get_future();
    future.wait();

    return status_ok;
}

void client()
{
    Client c;

    if(c.StartThreads() != status_ok)
    {
        ERROR_PRINT_LN("Start Threads failed");
        return;
    }

    INFO_PRINT_LN("Client shutting down");

    return;
}

int main()
{
    client();

    return 0;
}
