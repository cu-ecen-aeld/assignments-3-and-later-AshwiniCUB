#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)
#define MS_TO_US 1000
pthread_t newThread;

void* threadfunc(void* thread_param)
{
    int return_status;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    return_status = usleep((thread_func_args->wait_to_obtain_ms)*MS_TO_US);
	if(return_status != 0) {
		ERROR_LOG("usleep failed");
		return thread_param;
	}

    //lock mutex
    return_status = pthread_mutex_lock(thread_func_args->mutex_thread);
    if(return_status != 0) {
		ERROR_LOG("mutex_lock failed");
		return thread_param;
	}

    //delay before releasing the mutex
    return_status = usleep((thread_func_args->wait_to_obtain_ms)*MS_TO_US);
	if(return_status != 0) {
		ERROR_LOG("usleep failed");
		return thread_param;
	}

    //release mutex
    return_status = pthread_mutex_unlock(thread_func_args->mutex_thread);
    if(return_status != 0) {
		ERROR_LOG("mutex_unlock failed");
		return thread_param;
	}

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success=true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    int return_status;
    struct thread_data *thread_data_ptr = malloc(sizeof(struct thread_data));

    //initialization of thread_data structure
	thread_data_ptr->mutex_thread = mutex;
	thread_data_ptr->wait_to_obtain_ms=wait_to_obtain_ms;
	thread_data_ptr->wait_to_release_ms=wait_to_release_ms;
	thread_data_ptr->thread_complete_success= false;

    //thread creation
    return_status = pthread_create(&newThread, NULL, 
                                   &threadfunc,
                                   thread_data_ptr);

    if(return_status != 0){
        ERROR_LOG("pthread_create failed");
		return false;
    }

    *thread = newThread;               
    DEBUG_LOG("Thread exxecution is successful");

    return true;
}

