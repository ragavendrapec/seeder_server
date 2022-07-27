/*
 * util.h
 *
 *  Created on: 9 Jul 2022
 *  Author: vasanth ragavendran
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>

/*
 * Macro
 */
#define RED_COLOR                   "\x1b[31m"
#define GREEN_COLOR                 "\x1b[32m"
#define RESET_COLOR                 "\x1b[0m"

#define PRINT_ERROR                 (1<<0)
#define PRINT_INFO                  (1<<1)
#define PRINT_DEBUG                 (1<<2)

//#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO | PRINT_DEBUG)
#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO)
//#define PRINT_LOGS                  (PRINT_ERROR)

#define ERROR_PRINT_LN(...)         do{if(PRINT_LOGS & PRINT_ERROR) std::cout, RED_COLOR, "[error]", RESET_COLOR, __FUNCTION__, "[", __LINE__, "]: " __VA_ARGS__, std::endl;}while(0)
#define INFO_PRINT_LN(...)          do{if(PRINT_LOGS & PRINT_INFO) std::cout, __VA_ARGS__, std::endl;}while(0)
#define DEBUG_PRINT_LN(...)         do{if(PRINT_LOGS & PRINT_DEBUG) std::cout, "[debug]", __FUNCTION__, "[", __LINE__, "]: " __VA_ARGS__, std::endl;}while(0)


template <typename T>
std::ostream& operator,(std::ostream& out, const T&t)
{
    out << t;
    return out;
}

std::ostream& operator,(std::ostream& out, std::ostream&(*f)(std::ostream&))
{
    out << f;
    return out;
}

/*
 * Constants
 */
const std::string hello_msg("hello");
const std::string get_nodes_list_msg("nodes_list");
const std::string ping_msg("ping");
const std::string duration_alive_msg("alive_list");
const std::string peer_info_msg("peer_info_list");
const std::string shutting_down_msg("shutting down");
const int seeder_server_default_port = 54321;

/*
 * Type
 */
enum status_e {
    status_ok = 0,
    status_error = -1
};

template<typename T>
class Queue
{
public:
    Queue() = default;
    Queue(const Queue&) = delete; // disable copy
    Queue& operator=(const Queue&) = delete; // disable assignment

    void Push(T& data)
    {
        std::lock_guard<std::mutex> lock(receive_queue_mutex);
        receive_queue.push(data);
        receive_queue_cv.notify_all();
    }

    void Pop(T& data)
    {
        std::unique_lock<std::mutex> lock(receive_queue_mutex);
        receive_queue_cv.wait(lock, [this]()
            {
                return !receive_queue.empty();
            });
        if (!receive_queue.empty())
        {
            data = receive_queue.front();
            receive_queue.pop();
        }
    }

private:
    std::mutex receive_queue_mutex;
    std::queue<T> receive_queue;
    std::condition_variable receive_queue_cv;
};
/*
 * Function Declaration
 */

#endif // UTIL_H_
