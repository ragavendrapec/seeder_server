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
#define DEBUG_PRINT_LN(...)         std::cout, __FUNCTION__, "[", __LINE__, "]: " __VA_ARGS__, std::endl

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
