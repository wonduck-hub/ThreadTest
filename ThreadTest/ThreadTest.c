#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>
#include <threads.h>
#include <windows.h>

unsigned long long int sum = 0;
const int MIN_NUM = 1000000;
const int MAX_NUM = 5000000;

typedef struct TASLock {
	atomic_int state;
} TASLock;

typedef struct ThreadData { 
	TASLock* lock; 
	int start; 
	int end; 
} ThreadData;

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
	ThreadData* data = (ThreadData*)arg;

	for (int i = data->start; i <= data->end; ++i) { 
		tas_lock(data->lock);
		sum += i; 
		tas_unlock(data->lock);
	} 

	return 0;
}

int main(void) {
	clock_t start;
	clock_t end;
	
	start = clock();
	for (int i = MIN_NUM; i <= MAX_NUM; ++i) {
		sum += i;
	}
	end = clock();

	printf("Single thread time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("Sum: %llu\n", sum);

	SYSTEM_INFO sysinfo; 
	GetSystemInfo(&sysinfo); 
	int num_cores = sysinfo.dwNumberOfProcessors; // 현제 컴퓨터의 스레드 수
	thrd_t threads[2];

	printf("Number of cores: %d\n", num_cores);

	sum = 0;

	// 2개의 스레드를 사용한 TASLock 테스트
	TASLock lock;
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	ThreadData data[2] = { 
		{&lock, 1000000, 3000000}, 
		{&lock, 3000001, 5000000} 
	};

	for (int i = 0; i < 2; ++i) {
		if (thrd_create(&threads[i], tas_lock_test, &data[i]) != thrd_success) {
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
	printf("TASLock Sum: %llu\n", sum);
	sum = 0;
	
	return 0;
}