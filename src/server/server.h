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
    status_e StartThreads();

private:
    std::unique_ptr<std::thread> signal_thread;
//    std::unique_ptr<std::thread> socket_thread;
//    std::unique_ptr<std::thread> reply_thread;
    sigset_t sigset;
    std::atomic<bool> shutdown_requested;
};

#endif // SERVER_H_
