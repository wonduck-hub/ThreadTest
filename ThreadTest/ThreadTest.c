#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>
#include <threads.h>
#include <windows.h>

#define TWO (2)
#define FOUR (4)
#define EIGHT (8)
#define SIXTEEN (16)
#define THIRTYTWO (32)
#define SIXTYFOUR (64)

unsigned long long int sum = 0;
const int MIN_NUM = 1000000;
const int MAX_NUM = 5000000;

typedef struct Lock {
	atomic_int state;
} Lock;

typedef struct ThreadData { 
	Lock* lock; 
	int start; 
	int end; 
} ThreadData;

ThreadData init_ThreadData(Lock* l, int start, int end) {
	ThreadData d;
	d.lock = l;
	d.start = start;
	d.end = end;

	return d;
}

// Initialize the TASLock
void init_lock(Lock* lock) {
	atomic_init(&lock->state, 0);
}

// Acquire the TASLock using atomic_fetch_and_add

// 이 코드를 사용하면 데드락 문제와 값이 덮어쓰기되는 문제가 발생하지 않음
// atomic_fetch_add : 스레드끼리 값을 공유하면서 증가시키는 함수일 뿐이다
// 이때 값은 fetch와 add할 때 각각은 원자성을 보장하나 fetch와 add을 동시에 할 때는 원자성을 보장하지 않는다
// atomic_compare_exchange_waek : 이 함수는 변수의 현재 값과 기대 값(expected value)을 비교합니다. 
// 현재 값이 기대 값과 같으면 새 값(desired value)으로 교환하고, 그렇지 않으면 실패를 반환합니다. 
// 이 함수는 실패할 가능성이 있기 때문에 반복적인 시도에 적합합니다
// atomic_exchange : 변수의 값을 새 값으로 설정하고 이전 값을 원자적으로 반환(단일 교환 연산이 필요한 경우 사용한다)

void tas_lock(Lock* lock) {
	int expected = 0;
	while (atomic_compare_exchange_weak(&lock->state, &expected, 1)) {}
}

void ttas_lock(Lock* lock) {
	while (true) {
		// 첫 번째 테스트: 상태가 0인지 확인
		if (atomic_load(&lock->state) == 0) {
			// 두 번째 테스트: 비교 후 교환 시도
			int expected = 0;
			if (atomic_compare_exchange_weak(&lock->state, &expected, 1)) {
				break; // 락 획득 성공
			}
		}
		// 락 획득 실패 시 반복 시도
	}
}

void back_off_lock(Lock* lock) {
	while (true) {
		// 첫 번째 테스트: 상태가 0인지 확인
		if (atomic_load(&lock->state) == 0) {
			// 두 번째 테스트: 비교 후 교환 시도
			int expected = 0;
			if (atomic_compare_exchange_weak(&lock->state, &expected, 1)) {
				break; // 락 획득 성공
			}
		}
		// 락 획득 실패 후 일정 시간 대기
		Sleep(1);
	}
}

// Release the TASLock
void unlock(Lock* lock) {
	atomic_store(&lock->state, 0);
}

// Thread function
int tas_add(void* arg) {
	ThreadData* data = (ThreadData*)arg;

	for (int i = data->start; i <= data->end; ++i) { 
		tas_lock(data->lock);
		sum += i; 
		unlock(data->lock);
	} 

	return 0;
}

int ttas_add(void* arg) {
	ThreadData* data = (ThreadData*)arg;

	for (int i = data->start; i <= data->end; ++i) {
		ttas_lock(data->lock);
		sum += i;
		unlock(data->lock);
	}

	return 0;
}

int back_off_add(void* arg) {
	ThreadData* data = (ThreadData*)arg;

	for (int i = data->start; i <= data->end; ++i) {
		back_off_lock(data->lock);
		sum += i;
		unlock(data->lock);
	}

	return 0;
}

void tas_test() {
	// 2개의 스레드를 사용한 TASLock 테스트
	clock_t start;
	clock_t end;
	thrd_t threads[SIXTYFOUR];
	Lock lock;
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	ThreadData data[SIXTYFOUR];
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / TWO);
	for (int i = 1; i < TWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / TWO * i) + 1, 1000000 + (4000000 / TWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < TWO; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < TWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", TWO);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);

	// 4개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / FOUR);
	for (int i = 1; i < FOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / FOUR * i) + 1, 1000000 + (4000000 / FOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < FOUR; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < FOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", FOUR);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);

	// 8개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / EIGHT);
	for (int i = 1; i < EIGHT; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / EIGHT * i) + 1, 1000000 + (4000000 / EIGHT * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < EIGHT; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < EIGHT; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", EIGHT);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);

	// 16개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTEEN);
	for (int i = 1; i < SIXTEEN; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTEEN * i) + 1, 1000000 + (4000000 / SIXTEEN * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTEEN; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTEEN; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTEEN);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);

	// 32개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / THIRTYTWO);
	for (int i = 1; i < THIRTYTWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / THIRTYTWO * i) + 1, 1000000 + (4000000 / THIRTYTWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < THIRTYTWO; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < THIRTYTWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", THIRTYTWO);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);

	// 64개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTYFOUR);
	for (int i = 1; i < SIXTYFOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTYFOUR * i) + 1, 1000000 + (4000000 / SIXTYFOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTYFOUR; ++i) {
		if (thrd_create(&threads[i], tas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTYFOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTYFOUR);
	printf("TASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TASLock Sum: %llu\n", sum);
}

void ttas_test() {
	// 2개의 스레드를 사용한 TASLock 테스트
	clock_t start;
	clock_t end;
	thrd_t threads[SIXTYFOUR];
	Lock lock;
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	ThreadData data[SIXTYFOUR];
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / TWO);
	for (int i = 1; i < TWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / TWO * i) + 1, 1000000 + (4000000 / TWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < TWO; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < TWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", TWO);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 4개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / FOUR);
	for (int i = 1; i < FOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / FOUR * i) + 1, 1000000 + (4000000 / FOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < FOUR; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < FOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", FOUR);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 8개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / EIGHT);
	for (int i = 1; i < EIGHT; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / EIGHT * i) + 1, 1000000 + (4000000 / EIGHT * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < EIGHT; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < EIGHT; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", EIGHT);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 16개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTEEN);
	for (int i = 1; i < SIXTEEN; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTEEN * i) + 1, 1000000 + (4000000 / SIXTEEN * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTEEN; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTEEN; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTEEN);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 32개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / THIRTYTWO);
	for (int i = 1; i < THIRTYTWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / THIRTYTWO * i) + 1, 1000000 + (4000000 / THIRTYTWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < THIRTYTWO; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < THIRTYTWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", THIRTYTWO);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 64개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTYFOUR);
	for (int i = 1; i < SIXTYFOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTYFOUR * i) + 1, 1000000 + (4000000 / SIXTYFOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTYFOUR; ++i) {
		if (thrd_create(&threads[i], ttas_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTYFOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTYFOUR);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);
}

void back_off_test() {
	// 2개의 스레드를 사용한 TASLock 테스트
	clock_t start;
	clock_t end;
	thrd_t threads[SIXTYFOUR];
	Lock lock;
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	ThreadData data[SIXTYFOUR];
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / TWO);
	for (int i = 1; i < TWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / TWO * i) + 1, 1000000 + (4000000 / TWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < TWO; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < TWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", TWO);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 4개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / FOUR);
	for (int i = 1; i < FOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / FOUR * i) + 1, 1000000 + (4000000 / FOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < FOUR; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < FOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", FOUR);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 8개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / EIGHT);
	for (int i = 1; i < EIGHT; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / EIGHT * i) + 1, 1000000 + (4000000 / EIGHT * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < EIGHT; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < EIGHT; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", EIGHT);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 16개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTEEN);
	for (int i = 1; i < SIXTEEN; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTEEN * i) + 1, 1000000 + (4000000 / SIXTEEN * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTEEN; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTEEN; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTEEN);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 32개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / THIRTYTWO);
	for (int i = 1; i < THIRTYTWO; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / THIRTYTWO * i) + 1, 1000000 + (4000000 / THIRTYTWO * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < THIRTYTWO; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < THIRTYTWO; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", THIRTYTWO);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);

	// 64개의 스레드를 사용한 TASLock 테스트
	init_lock(&lock);

	// 각각의 스레드에 전달할 데이터 설정 
	data[0] = init_ThreadData(&lock, 1000000, 1000000 + 4000000 / SIXTYFOUR);
	for (int i = 1; i < SIXTYFOUR; ++i) {
		data[i] = init_ThreadData(&lock, 1000000 + (4000000 / SIXTYFOUR * i) + 1, 1000000 + (4000000 / SIXTYFOUR * (i + 1)));
	}

	sum = 0;
	for (int i = 0; i < SIXTYFOUR; ++i) {
		if (thrd_create(&threads[i], back_off_add, &data[i]) != thrd_success) {
			printf("Error creating thread 1\n");
			return;
		}
	}
	start = clock();
	for (int i = 0; i < SIXTYFOUR; ++i) {
		thrd_join(threads[i], NULL);
	}
	end = clock();

	printf("%d threads\n", SIXTYFOUR);
	printf("TTASLock Time: %f\n", (double)(end - start) / CLOCKS_PER_SEC);
	printf("TTASLock Sum: %llu\n", sum);
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
	
	printf("Number of cores: %d\n", num_cores);

	printf("\n===TASLock test===\n");
	tas_test();
	printf("\n===TTASLock test===\n");
	ttas_test();
	printf("\n===Back-off test===\n");
	back_off_test();

	return 0;
}