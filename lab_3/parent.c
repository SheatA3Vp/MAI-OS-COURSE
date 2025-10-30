#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_SIZE 4096

const char SHM_NAME[] = "/shared-memory";
const char SEM_NAME[] = "/semaphore";

static char CHILD_PROGRAM_NAME[] = "child";

int main(int argc, char **argv) {
	if (argc != 2) {
		char msg[1024];
		uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
		write(STDERR_FILENO, msg, len);
		_exit(EXIT_SUCCESS);
	}

	// NOTE: Get full path to the directory, where program resides
	char progpath[2048];
	{
		uint32_t size = sizeof(progpath);
		if (_NSGetExecutablePath(progpath, &size) != 0) {
			const char msg[] = "ERROR: Failed to get executable path\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			_exit(EXIT_FAILURE);
		}
		char* dir = dirname(progpath);
		strcpy(progpath, dir);
	}

	// NOTE: Clean up any existing shared memory and semaphore
	shm_unlink(SHM_NAME);
	sem_unlink(SEM_NAME);

	// NOTE: Create shared memory
	int shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (shm == -1) {
		const char msg[] = "ERROR: Failed to create SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	// NOTE: Resize shared memory
	if (ftruncate(shm, SHM_SIZE) == -1) {
		const char msg[] = "ERROR: Failed to resize SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	// NOTE: Map shared memory
	char *shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
	if (shm_buf == MAP_FAILED) {
		const char msg[] = "ERROR: Failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	// NOTE: Create semaphore
	sem_t *sem = sem_open(SEM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
	if (sem == SEM_FAILED) {
		const char msg[] = "ERROR: Failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	// NOTE: Spawn a new process
	const pid_t child = fork();

	switch (child) {
		case -1: { // NOTE: Kernel fails to create another process
			const char msg[] = "ERROR: Failed to spawn new process\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			_exit(EXIT_FAILURE);
		} break;

		case 0: { // NOTE: We're a child
			char path[4096];
			snprintf(path, sizeof(path) - 1, "%s/%s", progpath, CHILD_PROGRAM_NAME);

			// NOTE: args[0] must be a program name, next the actual arguments
			char *const args[] = {CHILD_PROGRAM_NAME, argv[1], NULL};

			int32_t status = execv(path, args);

			if (status == -1) {
				const char msg[] = "ERROR: Failed to exec into new executable image\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				_exit(EXIT_FAILURE);
			}
		} break;

		default: { // NOTE: We're a parent
			char buf[SHM_SIZE - sizeof(uint32_t)];
			ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf));
			if (bytes < 0) {
				const char msg[] = "ERROR: Failed to read from stdin\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				_exit(EXIT_FAILURE);
			}

			// NOTE: Send data to child via shared memory
			sem_wait(sem);
			// NOTE: Pointer to read data length
			uint32_t* length = (uint32_t*)shm_buf;
			// NOTE: Pointer to data
			char* data = shm_buf + sizeof(uint32_t);
			if (bytes > 0) {
				*length = (uint32_t)bytes;
				memcpy(data, buf, bytes);
				sem_post(sem);

				// NOTE: Wait for child to process and return result
				bool running = true;
				while (running) {
					sem_wait(sem);
					uint32_t len = *length;
					if (len >= SHM_SIZE) {
						ssize_t result_len = len - SHM_SIZE;
						*length = 0;
						// NOTE: Data was read, post the semaphore
						sem_post(sem);
						// Check if result starts with "ERROR:"
						const char error_prefix[] = "ERROR:";
						int output_fd = STDOUT_FILENO;
						if (result_len >= sizeof(error_prefix) - 1 && strncmp(data, error_prefix, sizeof(error_prefix) - 1) == 0) {
							output_fd = STDERR_FILENO;
						}
						// Output the result
						write(output_fd, data, result_len);
						running = false;
					} else {
						sem_post(sem);
					}
				}
				// NOTE: Signal child to exit
				sem_wait(sem);
				*length = UINT32_MAX;
				sem_post(sem);
			} else {
				// No input: signal child to exit
				*length = UINT32_MAX;
				sem_post(sem);
			}

			// NOTE: Wait for child to finish
			int status;
			if (wait(&status) == -1) {
				const char msg[] = "ERROR: Failed to wait for child\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				_exit(EXIT_FAILURE);
			}
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					_exit(EXIT_FAILURE);
				}
			} else {
				// NOTE: Child terminated abnormally
				_exit(EXIT_FAILURE);
			}
		} break;
	}

	sem_unlink(SEM_NAME);
	sem_close(sem);
	munmap(shm_buf, SHM_SIZE);
	shm_unlink(SHM_NAME);
	close(shm);
}