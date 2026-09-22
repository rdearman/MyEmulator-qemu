#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

static pthread_mutex_t atomic_lock = PTHREAD_MUTEX_INITIALIZER;

uint32_t __atomic_load_4(const volatile void *ptr, int order)
{
	uint32_t value;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	value = *(const volatile uint32_t *)ptr;
	pthread_mutex_unlock(&atomic_lock);
	return value;
}

void __atomic_store_4(volatile void *ptr, uint32_t value, int order)
{
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	*(volatile uint32_t *)ptr = value;
	pthread_mutex_unlock(&atomic_lock);
}

uint32_t __atomic_exchange_4(volatile void *ptr, uint32_t value, int order)
{
	uint32_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint32_t *)ptr;
	*(volatile uint32_t *)ptr = value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

uint32_t __atomic_fetch_add_4(volatile void *ptr, uint32_t value, int order)
{
	uint32_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint32_t *)ptr;
	*(volatile uint32_t *)ptr = previous + value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

uint32_t __atomic_fetch_sub_4(volatile void *ptr, uint32_t value, int order)
{
	uint32_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint32_t *)ptr;
	*(volatile uint32_t *)ptr = previous - value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

bool __atomic_compare_exchange_4(volatile void *ptr, void *expected,
		uint32_t desired, bool weak, int success_order, int failure_order)
{
	uint32_t current;
	bool matched;
	(void)weak;
	(void)success_order;
	(void)failure_order;
	pthread_mutex_lock(&atomic_lock);
	current = *(volatile uint32_t *)ptr;
	matched = current == *(uint32_t *)expected;
	if (matched)
		*(volatile uint32_t *)ptr = desired;
	else
		*(uint32_t *)expected = current;
	pthread_mutex_unlock(&atomic_lock);
	return matched;
}

uint64_t __atomic_load_8(const volatile void *ptr, int order)
{
	uint64_t value;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	value = *(const volatile uint64_t *)ptr;
	pthread_mutex_unlock(&atomic_lock);
	return value;
}

void __atomic_store_8(volatile void *ptr, uint64_t value, int order)
{
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	*(volatile uint64_t *)ptr = value;
	pthread_mutex_unlock(&atomic_lock);
}

uint64_t __atomic_fetch_add_8(volatile void *ptr, uint64_t value, int order)
{
	uint64_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint64_t *)ptr;
	*(volatile uint64_t *)ptr = previous + value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

uint64_t __atomic_fetch_sub_8(volatile void *ptr, uint64_t value, int order)
{
	uint64_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint64_t *)ptr;
	*(volatile uint64_t *)ptr = previous - value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

uint64_t __atomic_fetch_and_8(volatile void *ptr, uint64_t value, int order)
{
	uint64_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint64_t *)ptr;
	*(volatile uint64_t *)ptr = previous & value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

uint64_t __atomic_fetch_or_8(volatile void *ptr, uint64_t value, int order)
{
	uint64_t previous;
	(void)order;
	pthread_mutex_lock(&atomic_lock);
	previous = *(volatile uint64_t *)ptr;
	*(volatile uint64_t *)ptr = previous | value;
	pthread_mutex_unlock(&atomic_lock);
	return previous;
}

bool __atomic_compare_exchange_8(volatile void *ptr, void *expected,
		uint64_t desired, bool weak, int success_order, int failure_order)
{
	uint64_t current;
	bool matched;
	(void)weak;
	(void)success_order;
	(void)failure_order;
	pthread_mutex_lock(&atomic_lock);
	current = *(volatile uint64_t *)ptr;
	matched = current == *(uint64_t *)expected;
	if (matched)
		*(volatile uint64_t *)ptr = desired;
	else
		*(uint64_t *)expected = current;
	pthread_mutex_unlock(&atomic_lock);
	return matched;
}

bool __atomic_is_lock_free(unsigned int size, const volatile void *ptr)
{
	(void)size;
	(void)ptr;
	return false;
}
