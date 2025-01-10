#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 10

void printArray(int *array, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        printf("%d ", array[i]);
    }
    printf("\n");
}

int main() {
	// Динамическое выделение памяти
	int *dynamicArray = (int*)malloc(ARRAY_SIZE * sizeof(int));
	if (dynamicArray == NULL) {
	    return EXIT_FAILURE;
	}
    
    // Заполнение массива значениями
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        dynamicArray[i] = i + 1;
    }
    
    // Печать содержимого массива
    printArray(dynamicArray, ARRAY_SIZE);
    
    // Пример работы с указателями
    int value = 42;
    int *valuePtr = &value;
    printf("Значение через указатель: %d\n", *valuePtr);
    
    // Динамическое изменение размера массива
    int *resizedArray = (int*)realloc(dynamicArray, 2 * ARRAY_SIZE * sizeof(int));
    if (resizedArray == NULL) {
        perror("Ошибка изменения размера массива");
        exit(EXIT_FAILURE);
    } else {
        dynamicArray = resizedArray;
        
        // Заполняем новые элементы массива
        for (size_t i = ARRAY_SIZE; i < 2 * ARRAY_SIZE; ++i) {
            dynamicArray[i] = i + 1;
        }
        
        // Печать увеличенного массива
        printArray(dynamicArray, 2 * ARRAY_SIZE);
        
        // Освобождение памяти
        free(dynamicArray);
    }
    
    return EXIT_SUCCESS;
}

