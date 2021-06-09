#include <semaphore.h>
#define BS 16
#define INIT_BLK_CNT 5

typedef struct freed {
    int start;
    int length;
    struct freed *next;
    struct freed *prev;
} freed;


typedef struct block {
    char memory[BS];
    int  next_avail;
    freed *free_list;
    struct block *next;
    sem_t *locked;
} block;

class MemoryManager;
