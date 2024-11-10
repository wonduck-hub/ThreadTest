#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>
#include <threads.h>
#include <windows.h>

typedef struct TASLock {
	atomic_int state;
} TASLock;

// Initialize the TASLock
void init_lock(TASLock* lock) {
	atomic_init(&lock->state, 0);
}

// Acquire the TASLock using atomic_fetch_and_add
void tas_lock(TASLock* lock) {
	// Keep attempting to set state to 1 until the lock is acquired
	while (atomic_fetch_add(&lock->state, 1) != 0) {
		// Reset state back to 1 if lock acquisition failed
		atomic_fetch_add(&lock->state, -1);
		// Spin until the lock is acquired
	}
}

// Release the TASLock
void tas_unlock(TASLock* lock) {
	atomic_store(&lock->state, 0);
}

// Thread function
int tas_lock_test(void* arg) {
	unsigned long long int sum = 0;
	TASLock* lock = (TASLock*)arg; 
	init_lock(lock);

	for (int i = 1000000; i <= 5000000; ++i) { 
		tas_lock(lock);
		sum += i; 
		tas_unlock(lock);
	} 
	return 0;
}

int main(void) {
	unsigned long long int sum = 0;
	clock_t start;
	clock_t end;

	start = clock();
	for (int i = 1000000; i <= 5000000; ++i) {
		sum += i;
	}
	end = clock();

	printf("Sum: %llu\n", sum);
	printf("Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);

	SYSTEM_INFO sysinfo; 
	GetSystemInfo(&sysinfo); 
	int num_cores = sysinfo.dwNumberOfProcessors; // 현제 컴퓨터의 스레드 수
	thrd_t threads[2];

	printf("Number of cores: %d\n", num_cores);

	for (int i = 0; i < 2; ++i) {
		if (thrd_create(&threads[i], tas_lock_test, NULL) != thrd_success) {
			printf("Error creating thread 1\n"); 
			return 1;
		}
	}
	start = clock();
	for (int i = 0; i < 2; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	
	return 0;
}