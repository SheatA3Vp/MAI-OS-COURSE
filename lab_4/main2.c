#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h> // write
#include <dlfcn.h> // dlopen, dlsym, dlclose

#include "library.h" // NOTE: Custom function types

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

static gcd_func *gcd;
static sort_func *sort_ptr;

// NOTE: Functions stubs will be used, if library failed to load

// NOTE: Stubs are better than NULL function pointers,
//       you don't need to check for NULL before calling a function
static int gcd_stub(int a, int b) {
	(void)a; // NOTE: Compiler will warn about unused parameter otherwise
	(void)b;
	return 0;
}

static int* sort_stub(int* array, size_t n) {
	(void)array;
	(void)n;
	return array;
}

int main(void) {

	clock_t load_start = clock();
	void *lib1 = dlopen("./lib1.dylib", RTLD_LOCAL | RTLD_NOW);
	void *lib2 = dlopen("./lib2.dylib", RTLD_LOCAL | RTLD_NOW);
	clock_t load_end = clock();
	double load_time = (double)(load_end - load_start) / CLOCKS_PER_SEC;

	if (lib1 == NULL || lib2 == NULL) {
		const char msg[] = "error: failed to load libraries\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		return EXIT_FAILURE;
	}

	char load_msg[100];
	int load_len = snprintf(load_msg, sizeof(load_msg), "Libraries loaded in %f seconds\n\n", load_time);
	write(STDOUT_FILENO, load_msg, load_len);

	// По дефолту lib1
	gcd = dlsym(lib1, "gcd");
	sort_ptr = dlsym(lib1, "sort");

	if (gcd == NULL) gcd = gcd_stub;
	if (sort_ptr == NULL) sort_ptr = sort_stub;

	int current_lib = 1; // 1 или 2

	char buffer[1024];
	size_t pos = 0;
	while (true) {
		char c;
		ssize_t n = read(STDIN_FILENO, &c, 1);
		if (n <= 0) break;

		if (c == '\n') {
			buffer[pos] = '\0';

			// ======================== SWITCH LIBRARY ========================
			if (strcmp(buffer, "0") == 0) {
				if (current_lib == 1) {
					gcd = dlsym(lib2, "gcd");
					sort_ptr = dlsym(lib2, "sort");
					current_lib = 2;
				} else {
					gcd = dlsym(lib1, "gcd");
					sort_ptr = dlsym(lib1, "sort");
					current_lib = 1;
				}

				if (gcd == NULL) gcd = gcd_stub;
				if (sort_ptr == NULL) sort_ptr = sort_stub;
				char msg[50];
				int len = snprintf(msg, sizeof(msg), "Switched to library %d\n", current_lib);
				write(STDOUT_FILENO, msg, len);

			// ======================== GCD ========================
			} else if (buffer[0] == '1') {
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
				double time_taken = (double)(end - start) / CLOCKS_PER_SEC;

				char msg[150];
				int len = snprintf(msg, sizeof(msg), "GCD of %d and %d is %d\nTime taken: %f seconds\n", a, b, result, time_taken);
				write(STDOUT_FILENO, msg, len);

			// ======================== SORT ========================
			} else if (buffer[0] == '2') {
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
				sort_ptr(arr, n);
				clock_t end = clock();
				double time_taken = (double)(end - start) / CLOCKS_PER_SEC;

				const char prefix[] = "Sorted array: ";
				write(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
				for (size_t i = 0; i < n; ++i) {
					char num[20];
					int len = snprintf(num, sizeof(num), "%d%s", arr[i], i < n - 1 ? " " : "");
					write(STDOUT_FILENO, num, len);
				}
				char time_msg[50];
				int time_len = snprintf(time_msg, sizeof(time_msg), "\nTime taken: %f seconds\n", time_taken);
				write(STDOUT_FILENO, time_msg, time_len);

			// Неизвестная команда
			} else {
				const char msg[] = "Unknown command\n";
				write(STDOUT_FILENO, msg, sizeof(msg) - 1);
			}
			pos = 0;
		} else if (pos < sizeof(buffer) - 1) {
			buffer[pos++] = c;
		}
	}

	if (lib1) dlclose(lib1);
	if (lib2) dlclose(lib2);

	return 0;
}