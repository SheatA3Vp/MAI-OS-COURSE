 #include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char **argv) {
	char buf[4096];
	ssize_t bytes;

	pid_t pid = getpid();

	// NOTE: `O_WRONLY` only enables file for writing
	// NOTE: `O_CREAT` creates the requested file if absent
	// NOTE: `O_TRUNC` empties the file prior to opening
	// NOTE: `O_APPEND` subsequent writes are being appended instead of overwritten
	int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
	if (file == -1) {
		const char msg[] = "error: failed to open requested file\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}

	while ((bytes = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
		if (bytes < 0) {
			const char msg[] = "error: failed to read from stdin\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}

		static char line[4096];
		static int line_pos = 0;
		for (int i = 0; i < bytes; ++i) {
			if (buf[i] == '\n') {
				line[line_pos] = '\0';
				if (line_pos > 0) {

					float sum = 0.0f;
					char *ptr = line;
					char *endptr;
					while (*ptr) {
						if (isspace(*ptr)) {
							ptr++;
							continue;
						}
						float num = strtof(ptr, &endptr);

						if (ptr == endptr) {
							const char msg[] = "error: invalid character in input\n";
							write(STDERR_FILENO, msg, sizeof(msg));
							exit(EXIT_FAILURE);
						}
						sum += num;
						ptr = endptr;
					}

					char sum_str[64];
					int len = snprintf(sum_str, sizeof(sum_str), "%.2f\n", sum);
					int32_t written = write(file, sum_str, len);
					if (written != len) {
						const char msg[] = "error: failed to write to file\n";
						write(STDERR_FILENO, msg, sizeof(msg));
						exit(EXIT_FAILURE);
					}
				}
				line_pos = 0;
			} else {
				if (line_pos < (int)sizeof(line) - 1) {
					line[line_pos++] = buf[i];
				}
			}
		}
	}

	close(file);
}