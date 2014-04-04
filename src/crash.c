#include "crash.h"

void init_crasher()
{

	srand(time(NULL));
	long crash_sleep = 1+(int) (50.0*rand()/(RAND_MAX+1.0));

	crash_now = false;

	pthread_mutex_init(&(crash_mutex), NULL);
	if (pthread_create(&(crash_thread), NULL, crash_return, 
			   (void *)(crash_sleep)) != 0) {
		fprintf(stderr,"Didn't init crasher thread\n");
	}
	pthread_detach(crash_thread);
}

int crash_write(int vdisk, const void * buf, int num_bytes)
{
	pthread_mutex_lock(&(crash_mutex));
	if (false == crash_now){
		pthread_mutex_unlock(&(crash_mutex));
		return write(vdisk, buf, num_bytes);
	} else {
		pthread_mutex_unlock(&(crash_mutex));
		fprintf(stderr, "CRASH!!!!!\n");
		exit(-1);
	}
	pthread_mutex_unlock(&(crash_mutex));
	return 0;
}

void * crash_return(void * args) {
	long crash_sleep = (long)args;
	fprintf(stderr, "crash sleeping for %d\n", 
		crash_sleep * CRASHES_IN_100);

	sleep(crash_sleep * CRASHES_IN_100);
	
	pthread_mutex_lock(&(crash_mutex));
	crash_now = true;
	pthread_mutex_unlock(&(crash_mutex));

	return NULL;
}

