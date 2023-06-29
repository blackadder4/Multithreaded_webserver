#include <stdio.h>
#include <stdlib.h>
#include "request.h"
#include "io_helper.h"
#include <pthread.h>

char default_root[] = ".";

pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;

/*When you implement multithreading, the number of threads you create is configurable and is
passed to the server via the -t flag. Same for the size of the buffer which is passed via the -b
flag. In other words, you should not hardcode the number of threads to create and the size of
the buffer. Instead, you create those based on the values passed to main from the command
line.*/
int* buffer;
int buf_size;
int buf_count = 0;
int buf_start = 0;
int buf_end = 0;

void* worker_thread(void* arg) {
    int conn_fd;
    while (1) {
        pthread_mutex_lock(&buffer_lock);
        while (buf_count == 0) {
            pthread_cond_wait(&buffer_not_empty, &buffer_lock);
        }
        conn_fd = buffer[buf_start];
        buf_start = (buf_start + 1) % buf_size;
        buf_count--;
        pthread_mutex_unlock(&buffer_lock);
        request_handle(conn_fd);
        close_or_die(conn_fd);
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
    int num_threads = 4;

    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1) {
        switch (c) {
            case 'd':
                root_dir = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'b':
                buf_size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t num_threads] [-b buffer_size]\n");
                exit(1);
        }
    }

    // run out of this directory
    chdir_or_die(root_dir);

    // allocate buffer
    buffer = (int*) malloc(sizeof(int) * buf_size);

    // create worker threads
    pthread_t* threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    // now, get to work
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
        pthread_mutex_lock(&buffer_lock);
        while (buf_count == buf_size) {
            pthread_cond_wait(&buffer_not_empty, &buffer_lock);
        }
        buffer[buf_end] = conn_fd;
        buf_end = (buf_end + 1) % buf_size;
        buf_count++;
        pthread_mutex_unlock(&buffer_lock);
        pthread_cond_signal(&buffer_not_empty);
    }

    // free memory
    free(buffer);
    free(threads);

    return 0;
}
