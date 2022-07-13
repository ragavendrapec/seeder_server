/*
 * util.h
 *
 *  Created on: 9 Jul 2022
 *  Author: vasanth ragavendran
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <iostream>

/*
 * Macro
 */
#define PRINT_ERROR                 (1<<0)
#define PRINT_INFO                  (1<<1)
#define PRINT_DEBUG                 (1<<2)

//#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO | PRINT_DEBUG)
#define PRINT_LOGS                  (PRINT_ERROR | PRINT_INFO)
//#define PRINT_LOGS                  (PRINT_ERROR)

#define ERROR_PRINT_LN(...)         do{if(PRINT_LOGS & PRINT_ERROR) std::cout, "[error]", __FUNCTION__, "[", __LINE__, "]: " __VA_ARGS__, std::endl;}while(0)
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
const std::string get_nodes_list_msg("get nodes list");
const std::string ping_msg("ping");
const std::string duration_alive_msg("alive");
const std::string shutting_down_msg("shutting down");
const int seeder_server_default_port = 54321;

/*
 * Type
 */
enum status_e {
    status_ok = 0,
    status_error = -1
};

/*
 * Function Declaration
 */

#endif // UTIL_H_
