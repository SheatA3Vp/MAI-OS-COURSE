#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char **argv) {
	char buf[4096];
	ssize_t bytes;

	// NOTE: `O_WRONLY` only enables file for writing
	// NOTE: `O_CREAT` creates the requested file if absent
	// NOTE: `O_TRUNC` empties the file prior to opening
	// NOTE: `O_APPEND` subsequent writes are being appended instead of overwritten
	int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
	if (file == -1) {
		const char msg[] = "ERROR: Failed to open requested file\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		_exit(EXIT_FAILURE);
	}

	// NOTE: Read input data from standard input into the buffer
	bytes = read(STDIN_FILENO, buf, sizeof(buf) - 1);
	if (bytes < 0) {
		const char msg[] = "ERROR: Failed to read from stdin\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	buf[bytes] = '\0';

	float sum = 0.0f;
	int count = 0;
	char *ptr = buf;
	char *endptr;
	while (*ptr) {
		// NOTE: Skip whitespace characters in the input, but stop at newline
		if (isspace(*ptr) && *ptr != '\n') {
			ptr++;
			continue;
		}
		if (*ptr == '\n') {
			break;
		}
		float num = strtof(ptr, &endptr);

		if (num == HUGE_VALF || num == -HUGE_VALF) {
			const char msg[] = "ERROR: Number out of range\n";
			write(STDOUT_FILENO, msg, sizeof(msg) - 1);
			exit(EXIT_FAILURE);
		}

		if (ptr == endptr) {
			const char msg[] = "ERROR: Invalid character in input\n";
			write(STDOUT_FILENO, msg, sizeof(msg) - 1);
			exit(EXIT_FAILURE);
		}
		sum += num;
		count++;
		if (isinf(sum)) {
			const char msg[] = "ERROR: Sum overflow\n";
			write(STDOUT_FILENO, msg, sizeof(msg) - 1);
			exit(EXIT_FAILURE);
		}
		ptr = endptr;
	}

	if (count == 0) {
		const char msg[] = "ERROR: No numbers provided\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	// NOTE: Format the computed sum as a string and write it to file
	char sum_str[64];
	int len = snprintf(sum_str, sizeof(sum_str), "%.2f\n", sum);
	int32_t written = write(file, sum_str, len);
	if (written != len) {
		const char msg[] = "ERROR: Failed to write to file\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}

	// NOTE: Also write the sum to standard output
	int32_t stdout_written = write(STDOUT_FILENO, sum_str, len);
	if (stdout_written != len) {
		const char msg[] = "ERROR: Failed to write to stdout\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		exit(EXIT_FAILURE);
	}
	
	close(file);
}