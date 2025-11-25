#include "library.h"
#include "stddef.h"
#include "stdlib.h"

// NOTE: MSVC compiler does not export symbols unless annotated
#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#ifdef LIB1
EXPORT int gcd(int a, int b) {
	while (b != 0) {
		int t = b;
		b = a % b;
		a = t;
	}
	return a;
}

EXPORT int* sort(int* array, size_t n) {
	for (size_t i = 0; i < n - 1; ++i) {
		for (size_t j = 0; j < n - 1 - i; ++j) {
			if (array[j] > array[j + 1]) {
				int temp = array[j];
				array[j] = array[j + 1];
				array[j + 1] = temp;
			}
		}
	}
	return array;
}
#else
EXPORT int gcd(int a, int b) {
	int min = (a < b) ? a : b;
	for (int i = min; i > 0; --i) {
		if (a % i == 0 && b % i == 0) {
			return i;
		}
	}
	return 1;
}

EXPORT int* sort(int* array, size_t n) {
	if (n < 2) return array;

	int pivot_val = array[0];

	size_t less_count = 0;
	size_t equal_count = 0;
	size_t greater_count = 0;

	size_t less_size = 2;
	size_t equal_size = 2;
	size_t greater_size = 2;

	int* less = (int*)malloc(less_size * sizeof(int));
	int* equal_arr = (int*)malloc(equal_size * sizeof(int));
	int* greater = (int*)malloc(greater_size * sizeof(int));

	for (size_t i = 0; i < n; ++i) {

		if (array[i] < pivot_val) {
			if (less_count == less_size) {
				less_size *= 2;
				less = (int*)realloc(less, less_size * sizeof(int));
			}
			less[less_count++] = array[i];

		} else if (array[i] == pivot_val) {
			if (equal_count == equal_size) {
				equal_size *= 2;
				equal_arr = (int*)realloc(equal_arr, equal_size * sizeof(int));
			}
			equal_arr[equal_count++] = array[i];

		} else {
			if (greater_count == greater_size) {
				greater_size *= 2;
				greater = (int*)realloc(greater, greater_size * sizeof(int));
			}
			greater[greater_count++] = array[i];
		}
	}

	sort(less, less_count);
	sort(greater, greater_count);

	size_t idx = 0;
	for (size_t i = 0; i < less_count; ++i) array[idx++] = less[i];
	for (size_t i = 0; i < equal_count; ++i) array[idx++] = equal_arr[i];
	for (size_t i = 0; i < greater_count; ++i) array[idx++] = greater[i];

	free(less);
	free(equal_arr);
	free(greater);

	return array;
}
#endif

