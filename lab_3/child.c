#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_SIZE 4096

const char SHM_NAME[] = "/shared-memory";
const char SEM_NAME[] = "/semaphore";

int main(int argc, char **argv) {
	// NOTE: Open shared memory
	int shm = shm_open(SHM_NAME, O_RDWR, 0);
	if (shm == -1) {
		const char msg[] = "ERROR: Failed to open SHM\n";
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

	// NOTE: Open semaphore
	sem_t *sem = sem_open(SEM_NAME, O_RDWR);
	if (sem == SEM_FAILED) {
		const char msg[] = "ERROR: Failed to open semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	bool running = true;
	while (running) {
		sem_wait(sem);

		uint32_t *length = (uint32_t *)shm_buf;
		char *data = shm_buf + sizeof(uint32_t);

		if (*length == UINT32_MAX) {
			running = false;
		} else if (*length > 0 && *length < SHM_SIZE - sizeof(uint32_t)) {
			// NOTE: Parse and compute sum
			float sum = 0.0f;
			int count = 0;
			char *ptr = data;
			char *endptr;
			while (*ptr && ptr < data + *length) {
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
					memcpy(data, msg, sizeof(msg) - 1);
					// NOTE: Signal error to parent: set length = msg_len + SHM_SIZE (>= SHM_SIZE)
					*length = (sizeof(msg) - 1) + SHM_SIZE;
					sem_post(sem);
					_exit(EXIT_FAILURE);
				}

				if (ptr == endptr) {
					const char msg[] = "ERROR: Invalid character in input\n";
					memcpy(data, msg, sizeof(msg) - 1);
					*length = (sizeof(msg) - 1) + SHM_SIZE;
					sem_post(sem);
					_exit(EXIT_FAILURE);
				}
				sum += num;
				count++;
				if (isinf(sum)) {
					const char msg[] = "ERROR: Sum overflow\n";
					memcpy(data, msg, sizeof(msg) - 1);
					*length = (sizeof(msg) - 1) + SHM_SIZE;
					sem_post(sem);
					_exit(EXIT_FAILURE);
				}
				ptr = endptr;
			}

			if (count == 0) {
				const char msg[] = "ERROR: No numbers provided\n";
				memcpy(data, msg, sizeof(msg) - 1);
				*length = (sizeof(msg) - 1) + SHM_SIZE;
				sem_post(sem);
				_exit(EXIT_FAILURE);
			}

			// NOTE: Open file for writing
			int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
			if (file == -1) {
				const char msg[] = "ERROR: Failed to open requested file\n";
				memcpy(data, msg, sizeof(msg) - 1);
				*length = (sizeof(msg) - 1) + SHM_SIZE;
				sem_post(sem);
				_exit(EXIT_FAILURE);
			}

			// NOTE: Format sum as string
			char sum_str[64];
			int len = snprintf(sum_str, sizeof(sum_str), "%.2f\n", sum);

			// NOTE: Write sum to file
			int32_t written = write(file, sum_str, len);
			if (written != len) {
				const char msg[] = "ERROR: Failed to write to file\n";
				memcpy(data, msg, sizeof(msg) - 1);
				*length = (sizeof(msg) - 1) + SHM_SIZE;
				sem_post(sem);
				_exit(EXIT_FAILURE);
			}
			close(file);

			// NOTE: Send result to parent via shared memory
			*length = len + SHM_SIZE;
			memcpy(data, sum_str, len);
		}
		sem_post(sem);
	}

	sem_close(sem);
	munmap(shm_buf, SHM_SIZE);
	close(shm);
	return 0;
}