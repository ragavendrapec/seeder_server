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
private:
    std::unique_ptr<std::thread> socket_thread;
    std::unique_ptr<std::thread> reply_thread;
    std::unique_ptr<std::thread> signal_thread;
};

#endif // SERVER_H_
