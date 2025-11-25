#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

#include "library.h"

// Проверка корректного long без переполнения
bool parse_long(const char *str, long *out, long min, long max) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    if (endptr == str) return false;
    while (*endptr && isspace(*endptr)) ++endptr;
    if (*endptr != '\0') return false;

    if ((val == LONG_MAX || val == LONG_MIN) && errno == ERANGE)
        return false;

    if (val < min || val > max)
        return false;

    *out = val;
    return true;
}

// Парсинг следующего числа
bool parse_next_number(char **p_ptr, long *out, long min, long max) {
    char *p = *p_ptr;

    while (*p == ' ') ++p;
    if (*p == '\0') return false;

    char tmp[32];
    int i = 0;
    while (*p && *p != ' ') {
        if (i >= (int)sizeof(tmp)-1) return false;
        tmp[i++] = *p++;
    }
    tmp[i] = '\0';

    if (!parse_long(tmp, out, min, max))
        return false;

    *p_ptr = p;
    return true;
}

// Функции из динамической библиотеки
extern int gcd(int a, int b);
extern int* sort(int* array, size_t n);


int main(void) {
    char buffer[1024];
    size_t pos = 0;

    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == '\n') {
            buffer[pos] = '\0';

            // ======================== GCD ========================
            if (buffer[0] == '1') {
                char *p = buffer + 1;

                long a_val, b_val;

                if (!parse_next_number(&p, &a_val, 1, INT_MAX)) {
                    write(STDOUT_FILENO,
                          "Invalid input for gcd: first number must be positive integer\n",
                          sizeof("Invalid input for gcd: first number must be positive integer\n") - 1);
                    write(STDOUT_FILENO, "\n", 1);
                    pos = 0; 
                    continue;
                }

                if (!parse_next_number(&p, &b_val, 1, INT_MAX)) {
                    write(STDOUT_FILENO,
                          "Invalid input for gcd: second number must be positive integer\n",
                          sizeof("Invalid input for gcd: second number must be positive integer\n") - 1);
                    write(STDOUT_FILENO, "\n", 1);
                    pos = 0; 
                    continue;
                }

                while (*p == ' ') ++p;
                if (*p != '\0') {
                    write(STDOUT_FILENO,
                          "Invalid input for gcd: extra characters after numbers\n",
                          sizeof("Invalid input for gcd: extra characters after numbers\n") - 1);
                    write(STDOUT_FILENO, "\n", 1);
                    pos = 0; 
                    continue;
                }

                int a = (int)a_val;
                int b = (int)b_val;

                clock_t start = clock();
                int result = gcd(a, b);
                clock_t end = clock();

                double t = (double)(end - start) / CLOCKS_PER_SEC;

                char msg[200];
                int len = snprintf(msg, sizeof(msg),
                                   "GCD of %d and %d is %d\n"
                                   "Time taken: %f seconds\n\n",
                                   a, b, result, t);
                write(STDOUT_FILENO, msg, len);
            }

            // ======================== SORT ========================
            else if (buffer[0] == '2') {
                char *p = buffer + 1;
                int arr[100];
                size_t n = 0;

                long v;
                while (parse_next_number(&p, &v, INT_MIN, INT_MAX)) {
                    arr[n++] = (int)v;
                    if (n == 100) break;
                }

                while (*p == ' ') ++p;
                if (n == 100 && *p != '\0') {
                    write(STDOUT_FILENO,
                          "Buffer full, last number read: ",
                          sizeof("Buffer full, last number read: ") - 1);
                    char num[32];
                    int len = snprintf(num, sizeof(num), "%d\n", arr[99]);
                    write(STDOUT_FILENO, num, len);
                } else if (n == 0) {
                    write(STDOUT_FILENO,
                          "Invalid input for sort: at least one valid integer required\n",
                          sizeof("Invalid input for sort: at least one valid integer required\n") - 1);
                    write(STDOUT_FILENO, "\n", 1);
                    pos = 0; 
                    continue;
                } else if (*p != '\0') {
                    write(STDOUT_FILENO,
                          "Invalid input for sort: extra characters after numbers\n",
                          sizeof("Invalid input for sort: extra characters after numbers\n") - 1);
                    write(STDOUT_FILENO, "\n", 1);
                    pos = 0; 
                    continue;
                }

                clock_t start = clock();
                sort(arr, n);
                clock_t end = clock();
                double t = (double)(end - start) / CLOCKS_PER_SEC;

                write(STDOUT_FILENO, "Sorted array: ", sizeof("Sorted array: ") - 1);
                for (size_t i = 0; i < n; ++i) {
                    char num[32];
                    int len = snprintf(num, sizeof(num), "%d%s", arr[i], i < n - 1 ? " " : "");
                    write(STDOUT_FILENO, num, len);
                }

                char msg[100];
                int len = snprintf(msg, sizeof(msg),
                                   "\nTime taken: %f seconds\n\n", t);
                write(STDOUT_FILENO, msg, len);
            }

            // Неизвестная команда
            else {
                write(STDOUT_FILENO, "Unknown command\n\n", sizeof("Unknown command\n\n") - 1);
            }

            pos = 0;

        } else if (pos < sizeof(buffer) - 1) {
            buffer[pos++] = c;
        }
    }

    return 0;
}
