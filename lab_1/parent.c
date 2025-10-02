#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

static char SERVER_PROGRAM_NAME[] = "child";

int main(int argc, char **argv) {
	if (argc == 1) {
		char msg[1024];
		uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
		write(STDERR_FILENO, msg, len);
		exit(EXIT_SUCCESS);
	}

	// NOTE: Get full path to the directory, where program resides
	char progpath[2048];
	{
		// NOTE: Read full program path, including its name
		ssize_t len = readlink("/proc/self/exe", progpath,
		                       sizeof(progpath) - 1);
		if (len == -1) {
			const char msg[] = "Failed to read full program path\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}

		// NOTE: Trim the path to first slash from the end
		while (progpath[len] != '/')
			--len;

		progpath[len] = '\0';
	}

	// NOTE: Open pipes
	int parent_to_child[2];
	if (pipe(parent_to_child) == -1) {
		const char msg[] = "Failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}


	int child_to_parent[2];
	if (pipe(child_to_parent) == -1) {
		const char msg[] = "Failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}

	// NOTE: Spawn a new process
	const pid_t child = fork();

	switch (child) {
	case -1: { // NOTE: Kernel fails to create another process
		const char msg[] = "Failed to spawn new process\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	} break;

	case 0: { // NOTE: We're a child
		close(parent_to_child[1]);

		dup2(parent_to_child[0], STDIN_FILENO);
		close(parent_to_child[0]);

		dup2(child_to_parent[1], STDOUT_FILENO);
		close(child_to_parent[1]);

		{
			char path[4096];
			snprintf(path, sizeof(path) - 1, "%s/%s", progpath, SERVER_PROGRAM_NAME);

			// NOTE: args[0] must be a program name, next the actual arguments
			// NOTE: `NULL` at the end is mandatory, because `exec*`
			//       expects a NULL-terminated list of C-strings
			char *const args[] = {SERVER_PROGRAM_NAME, argv[1], NULL};

			int32_t status = execv(path, args);

			if (status == -1) {
				const char msg[] = "Failed to exec into new exectuable image\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				exit(EXIT_FAILURE);
			}
		}
	} break;

	default: { // NOTE: We're a parent

		char buf[4096];
		ssize_t bytes;

		while (bytes = read(STDIN_FILENO, buf, sizeof(buf))) {
			if (bytes < 0) {
				const char msg[] = "Failed to read from stdin\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				exit(EXIT_FAILURE);
			}

			write(parent_to_child[1], buf, bytes);
			break;  // NOTE: Exit after first input
		}

		close(parent_to_child[1]);
		close(child_to_parent[1]);

		char result_buf[4096];
		ssize_t result_bytes = read(child_to_parent[0], result_buf, sizeof(result_buf));
		if (result_bytes > 0) {
			// NOTE: Check if the output from child starts with "ERROR: " prefix
			// If yes, it's an error message, so write to STDERR; otherwise, it's the result, write to STDOUT
			const char error_prefix[] = "ERROR: ";
			int output_fd = STDOUT_FILENO;
			size_t offset = 0;
			if (result_bytes >= sizeof(error_prefix) - 1 && strncmp(result_buf, error_prefix, sizeof(error_prefix) - 1) == 0) {
				output_fd = STDERR_FILENO;
				offset = sizeof(error_prefix) - 1;  // Skip the "ERROR: " prefix when writing
			}
			write(output_fd, result_buf + offset, result_bytes - offset);
		} else if (result_bytes < 0) {
			const char msg[] = "Failed to read from child pipe\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}

		close(child_to_parent[0]);

		int status;
		if (wait(&status) == -1) {
			const char msg[] = "Failed to wait for child\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0) {
				exit(EXIT_FAILURE);
			}
		} else {
			// NOTE: Child terminated abnormally
			exit(EXIT_FAILURE);
		}
	} break;
	}
}