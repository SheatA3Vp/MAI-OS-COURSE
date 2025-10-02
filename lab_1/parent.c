#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

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
			const char msg[] = "error: failed to read full program path\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}

		// NOTE: Trim the path to first slash from the end
		while (progpath[len] != '/')
			--len;

		progpath[len] = '\0';
	}

	// NOTE: Open pipes
	int parent_to_child[2]; // AB
	if (pipe(parent_to_child) == -1) {
		const char msg[] = "error: failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}


	int child_to_parent[2]; // BA
	if (pipe(child_to_parent) == -1) {
		const char msg[] = "error: failed to create pipe\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}

	// NOTE: Spawn a new process
	const pid_t child = fork();

	switch (child) {
	case -1: { // NOTE: Kernel fails to create another process
		const char msg[] = "error: failed to spawn new process\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	} break;

	case 0: { // NOTE: We're a child, child doesn't know its pid after fork
		{
			pid_t pid = getpid(); // NOTE: Get child PID

			char msg[64];
			const int32_t length = snprintf(msg, sizeof(msg),
				"%d: I'm a child\n", pid);
			write(STDOUT_FILENO, msg, length);
		}

		close(parent_to_child[1]); //не пишем в ребенка

		dup2(parent_to_child[0], STDIN_FILENO); //чтение из родителя = вводу
		close(parent_to_child[0]);

		dup2(child_to_parent[1], STDOUT_FILENO); //запись в родителя равно выводу
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
				const char msg[] = "error: failed to exec into new exectuable image\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				exit(EXIT_FAILURE);
			}
		}
	} break;

	default: { // NOTE: We're a parent, parent knows PID of child after fork
		{
			pid_t pid = getpid(); // NOTE: Get parent PID

			char msg[64];
			const int32_t length = snprintf(msg, sizeof(msg),
				"%d: I'm a parent, my child has PID %d\n", pid, child);
			write(STDOUT_FILENO, msg, length);
		}

		close(parent_to_child[0]); //не читаем из родителя 

		char buf[4096];
		ssize_t bytes;

		while (bytes = read(STDIN_FILENO, buf, sizeof(buf))) {
			if (bytes < 0) {
				const char msg[] = "error: failed to read from stdin\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				exit(EXIT_FAILURE);
			} else if (buf[0] == '\n') {
				// NOTE: When Enter is pressed with no input, then exit client
				break;
			}

			write(parent_to_child[1], buf, bytes);
		}

		close(parent_to_child[1]);

		wait(NULL);
	} break;
	}
}