/*
 * server.cpp
 *
 *  Created on: 9 Jul 2022
 *  Author: Vasanth Ragavendran
 */
#include <iostream>
#include <mutex>
#include <condition_variable>

#include "server.h"

/*
 * Local Macro
 */


/*
 * Local Constants
 */


/*
 * Globals
 */
std::mutex cv_mutex;
std::condition_variable cv;
bool signal_received = false;

SeederServer::SeederServer()
{
    signal_received = false;
    shutdown_requested.store(false);

    signal_thread = nullptr;
}

SeederServer::~SeederServer()
{
    if (signal_thread)
    {
        signal_thread->join();
    }
}

status_e SeederServer::BlockSignals()
{
    int ret;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGQUIT);

    ret = pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    if (ret != 0)
    {
        std::cout << "sigmask failed" << std::endl;
        return status_error;
    }

    return status_ok;
}

void SeederServer::ReceiveSignal()
{
    int signal;
    struct timespec t;

    std::cout << "Receive signal thread created" << std::endl;

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
            std::cout << "Signal received " << signal << std::endl;
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lguard(cv_mutex);
        signal_received = true;
        cv.notify_all();
    }

    std::cout << "Issued notify all" << std::endl;
}

status_e SeederServer::StartThreads()
{
    signal_thread.reset(new std::thread(&SeederServer::ReceiveSignal, this));

    return status_ok;
}

void server()
{
    SeederServer seeder_server;

    if(seeder_server.BlockSignals() != status_ok)
    {
        std::cout << "Block signals failed" << std::endl;
        return;
    }

    if(seeder_server.StartThreads() != status_ok)
    {
        std::cout << "Start Threads failed" << std::endl;
        return;
    }

    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        cv.wait(lock, [](){ return signal_received; });
    }

    std::cout << "Shutting down" << std::endl;
    return;
}

int main()
{
    server();
}
