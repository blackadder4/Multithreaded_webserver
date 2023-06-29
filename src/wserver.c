#include <stdio.h>
#include <stdlib.h>
#include "request.h"
#include "io_helper.h"
#include <pthread.h>

char default_root[] = ".";
//defined the FIFO buffer 
pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;
int* buffer;
int buffers;
int buf_count = 0;
int buf_start = 0;
int buf_end = 0;
// after the dispatched recieve it hand it to this woker thread
// Have the threads get a value from the buffer (using the get()) method, and process it.
//Create t threads (based on the value of the -t flag).  There is no loops variable here like in the module.  The threads should be running forever (while(true) { })
void* worker_thread(void* arg) {
    int conn_fd;
    while (1) {
        pthread_mutex_lock(&buffer_lock);
        while (buf_count == 0) {
            pthread_cond_wait(&buffer_not_empty, &buffer_lock);
        }
        conn_fd = buffer[buf_start];
        buf_start = (buf_start + 1) % buffers;
        buf_count--;
        pthread_mutex_unlock(&buffer_lock);
        request_handle(conn_fd);
        close_or_die(conn_fd);
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *basedir = default_root;
    int port = 10000;
    int num_threads = 4;
//    if(num_threads < 1){
    	//kill server if the input is invalid
//    	exit(1);
//    }
    while ((c = getopt(argc, argv, "d:p:t:b:s")) != -1) {
        switch (c) {
            case 'd':
            	// this is the path so its a string
                basedir = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                // since port can be negative I will not check here
                break;
            case 't':
                num_threads = atoi(optarg);
                if(num_threads < 1){
                	printf(stderr, "invalid thread\n");
                	exit(1);
                }
                if(num_threads <= 2){
                	num_threads += 2;
                }
                break;
            case 'b':
                buffers = atoi(optarg);
                if(buffers < 1){
                	printf(stderr, "buffer is too small\n");
                	exit(1);
                }
                if(buffers < num_threads){
                	// if the buffer is too small forcefully use same size as number threads 
                	buffers = num_threads;
                }
                break;
            case 's':
                //as per comment scheduling algor only needs FIFO so nothing is implemented here
            default:
                fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t num_threads] [-b buffer_size] [-s scheduling is disabled]\n");
                exit(1);
        }
    }

    // check if number of threads is valid
    ///if (num_threads <= 0) {
    //    fprintf(stderr, "error: invalid number of threads specified\n");
    //    exit(1);
    //}
    
    // check if number of threads is valid for fixed thread pool

    // run out of this directory
    chdir_or_die(basedir);

    // allocate buffer
    buffer = (int*) malloc(sizeof(int) * buffers);
    if (buffer == NULL) {
        fprintf(stderr, "error: failed to allocate buffer\n");
        exit(1);
    }

    // create worker threads
    pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);

    // this part creats a thread pool, for i less than num threads create this many threads 
    int num_created_threads = 0;
    for (int i = 0; i < num_threads; i++) {
    	// point to threads, use null policy which is default fifo on pop os
    	//create a worker_thread, pass null to the start routine
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) == 0) {
            num_created_threads++;
        } else {
            fprintf(stderr, "error: failed to create a worker thread %d\n", i);
        }
    }

    // check if all threads were created
    if (num_created_threads < num_threads) {
        fprintf(stderr, "error: failed to create all the worker threads\n");
        exit(1);
    }
    printf("fixed pool for size :  %d\n", num_created_threads);

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
        pthread_mutex_lock(&buffer_lock);
        while (buf_count == buffers) {
            pthread_cond_wait(&buffer_not_empty, &buffer_lock);
        }
        buffer[buf_end] = conn_fd;
        buf_end = (buf_end + 1) % buffers;
        buf_count++;
        pthread_mutex_unlock(&buffer_lock);
        pthread_cond_signal(&buffer_not_empty);
    }

    // free memory
    free(buffer);
    free(threads);

    return 0;
}

