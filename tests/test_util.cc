// Utilities for running tests
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>

std::mutex print_lock;
typedef std::lock_guard<std::mutex> mutex_guard;

// Prints a message if the two items aren't equal
template <class T>
inline void do_expect_equal(T x, const char *xname, T y, const char *yname, size_t line) {
    if (x != y) {
        mutex_guard m(print_lock);
        std::cout << "ERROR:\t" << xname << "(" << x << ") does not equal " << yname << "(" << y << ") on line " << line << std::endl;
    }
}
#define EXPECT_EQ(x, y) do_expect_equal(x, #x, y, #y, __LINE__)

// Prints a message if the two items are equal
template <class T>
inline void do_expect_not_equal(T x, const char *xname, T y, const char *yname, size_t line) {
    if (x == y) {
        mutex_guard m(print_lock);
        std::cout << "ERROR:\t" << xname << "(" << x << ") equals " << yname << "(" << y << ") on line " << line << std::endl;
    }
}
#define EXPECT_NE(x, y) do_expect_not_equal(x, #x, y, #y, __LINE__)

// Prints a message if the item is false
inline void do_expect_true(bool x, const char *xname, size_t line) {
    if (!x) {
        mutex_guard m(print_lock);
        std::cout << "ERROR:\t" << xname << "(" << x << ") is false on line " << line << std::endl;
    }
}
#define EXPECT_TRUE(x) do_expect_true(x, #x, __LINE__)

// Prints a message if the item is true
inline void do_expect_false(bool x, const char *xname, size_t line) {
    if (x) {
        mutex_guard m(print_lock);
        std::cout << "ERROR:\t" << xname << "(" << x << ") is true on line " << line << std::endl;
    }
}
#define EXPECT_FALSE(x) do_expect_false(x, #x, __LINE__)

// Prints a message and exists if the two items aren't equal
template <class T>
inline void do_assert_equal(T x, const char *xname, T y, const char *yname, size_t line) {
    if (x != y) {
        mutex_guard m(print_lock);
        std::cout << "FATAL ERROR:\t" << xname << "(" << x << ") does not equal " << yname << "(" << y << ") on line " << line << std::endl;
        exit(1);
    }
}
#define ASSERT_EQ(x, y) do_assert_equal(x, #x, y, #y, __LINE__)

// Prints a message and exists if the item is false
inline void do_assert_true(bool x, const char *xname, size_t line) {
    if (!x) {
        mutex_guard m(print_lock);
        std::cout << "FATAL ERROR:\t" << xname << "(" << x << ") is false on line " << line << std::endl;
        exit(1);
    }
}
#define ASSERT_TRUE(x) do_assert_true(x, #x, __LINE__)


// Parses boolean flags and flags with positive integer arguments
void parse_flags(int argc, char**argv, const char* description,
                 const char* args[], size_t* arg_vars[], const char* arg_help[], size_t arg_num,
                 const char* flags[], bool* flag_vars[], const char* flag_help[], size_t flag_num) {

    errno = 0;
    for (int i = 0; i < argc; i++) {
        for (size_t j = 0; j < arg_num; j++) {
            if (strcmp(argv[i], args[j]) == 0) {
                if (i == argc-1) {
                    std::cerr << "You must provide a positive integer argument after the " << args[j] << " argument" << std::endl;
                    exit(1);
                } else {
                    size_t argval = strtoull(argv[i+1], NULL, 10);
                    if (errno != 0) {
                        std::cerr << "The argument to " << args[j] << " must be a valid size_t" << std::endl;
                        exit(1);
                    } else {
                        *(arg_vars[j]) = argval;
                    }
                }
            }
        }
        for (size_t j = 0; j < flag_num; j++) {
            if (strcmp(argv[i], flags[j]) == 0) {
                *(flag_vars[j]) = true;
            }
        }
        if (strcmp(argv[i], "--help") == 0) {
            std::cerr << description << std::endl;
            std::cerr << "Arguments:" << std::endl;
            for (size_t j = 0; j < arg_num; j++) {
                std::cerr << args[j] << " (default " << *arg_vars[j] << "):\t" << arg_help[j] << std::endl;
            }
            for (size_t j = 0; j < flag_num; j++) {
                std::cerr << flags[j] << " (default " << (*flag_vars[j] ? "true" : "false") << "):\t" << flag_help[j] << std::endl;
            }
            exit(0);
        }
    }
}

/* generateKey is a function from a number to another given type, used
 * to generate keys for insertion. */
template <class T>
T generateKey(size_t i) {
    return (T)i;
}
/* This specialization returns a stringified representation of the
 * given integer, where the number is copied to the end of a long
 * string of 'a's, in order to make comparisons and hashing take
 * time. */
template <>
std::string generateKey<std::string>(size_t i) {
    const size_t min_length = 100;
    const std::string num(std::to_string(i));
    if (num.size() >= min_length) {
        return num;
    }
    std::string ret(min_length, 'a');
    const size_t startret = min_length - num.size();
    for (size_t i = 0; i < num.size(); i++) {
        ret[i+startret] = num[i];
    }
    return ret;
}
