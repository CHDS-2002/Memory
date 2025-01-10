#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <aio.h>

#define BUFFER_SIZE 1024
#define NUM_THREADS 4
#define FILE_PATH "/path/to/your/file"

typedef struct {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    int done;
} shared_data_t;

shared_data_t shared_data;

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void *thread_function(void *arg) {
    int thread_id = *(int *)arg;
    printf("Thread %d started.\n", thread_id);
    
    while (1) {
        pthread_mutex_lock(&shared_data.mutex);
        while (!shared_data.done && shared_data.bytes_read <= 0) {
            pthread_cond_wait(&shared_data.cond_var, &shared_data.mutex);
        }
        if (shared_data.done) {
            break;
        }
        
        // Копирование данных в локальный буфер
        char local_buffer[BUFFER_SIZE];
        strncpy(local_buffer, shared_data.buffer, shared_data.bytes_read);
        local_buffer[shared_data.bytes_read] = '\0';
        
        // Освобождение мьютекса перед обработкой данных
        pthread_mutex_unlock(&shared_data.mutex);
        
        // Обработка данных
        printf("Thread %d processing data: %s\n", thread_id, local_buffer);
        
        // Ждем немного перед следующей итерацией
        usleep(500000); // 0.5 секунды
    }
    
    pthread_mutex_unlock(&shared_data.mutex);
    printf("Thread %d finished.\n", thread_id);
    return NULL;
}

void async_io_handler(union sigval sv) {
    struct aiocb *cb = (struct aiocb *)sv.sival_ptr;
    ssize_t ret = aio_return(cb);
    if (ret < 0) {
        error_exit("async_io_handler: read failed");
    }
    
    // Получаем данные из результата операции чтения
    memcpy(shared_data.buffer, cb->aio_buf, ret);
    shared_data.bytes_read = ret;
    shared_data.done = (ret == 0); // Флаг окончания чтения файла
    
    // Уведомляем потоки о наличии новых данных
    pthread_mutex_lock(&shared_data.mutex);
    pthread_cond_broadcast(&shared_data.cond_var);
    pthread_mutex_unlock(&shared_data.mutex);
}

int main() {
    // Инициализация общих данных
    memset(&shared_data, 0, sizeof(shared_data));
    pthread_mutex_init(&shared_data.mutex, NULL);
    pthread_cond_init(&shared_data.cond_var, NULL);
    
    // Создание потоков
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]) != 0) {
            error_exit("pthread_create");
        }
    }
    
    // Открытие файла для асинхронного чтения
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd == -1) {
        error_exit("open");
    }
    
    // Установка сигнала для обработки завершения асинхронных операций
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = async_io_handler;
    sa.sa_flags = SA_SIGINFO;
    union sigval sv;
    sv.sival_ptr = NULL; // Указываем на структуру aiocb
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        error_exit("sigaction");
    }
    
    // Асинхронное чтение данных из файла
    struct aiocb cb;
    memset(&cb, 0, sizeof(cb));
    cb.aio_fildes = fd;
    cb.aio_buf = malloc(BUFFER_SIZE);
    cb.aio_nbytes = BUFFER_SIZE;
    cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    cb.aio_sigevent.sigev_signo = SIGRTMIN;
    cb.aio_sigevent.sigev_value.sival_ptr = &cb;
    
    while (1) {
        // Очистка буфера перед новым чтением
        memset(cb.aio_buf, 0, BUFFER_SIZE);
        
        // Выполнение асинхронного чтения
        if (aio_read(&cb) == -1) {
            error_exit("aio_read");
        }
        
        // Ожидание завершения операции чтения
        while (aio_error(&cb) == EINPROGRESS) {
            pause();
        }
        
        // Завершение цикла, если достигнут конец файла
        if (aio_return(&cb) == 0 || shared_data.done) {
            break;
        }
    }
    
    close(fd);
    free(cb.aio_buf);
    
    // Уведомление потоков о завершении работы
    pthread_mutex_lock(&shared_data.mutex);
    shared_data.done = 1;
    pthread_cond_broadcast(&shared_data.cond_var);
    pthread_mutex_unlock(&shared_data.mutex);
    
    // Ожидание завершения всех потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            error_exit("pthread_join");
        }
    }
    
    // Освобождение ресурсов
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond_var);
    
    printf("All threads completed successfully.\n");
    return EXIT_SUCCESS;
}