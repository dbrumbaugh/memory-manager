#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>


#include "manager.h"

block *_block_create()
{
    block *new_blk = (block *) calloc(1, sizeof(block));
    new_blk->locked = (sem_t *) calloc(1, sizeof(sem_t));
    sem_init(new_blk->locked, 0, 1);

    return new_blk;
}


freed *_create_freed(int start, int len)
{
    freed *new_freed = (freed *) calloc(1, sizeof(freed));
    new_freed->start = start;
    new_freed->length = len;

    return new_freed;
}


int block_avail_space(block *blk)
{
    return BS - blk->next_avail;
}


class MemoryManager {
    private:
        block *first_block;

        void *allocate(block *blk, int size, int position)
        {
            void *ptr;
            if (position == -1) { // stick at end
                ptr = &(blk->memory[blk->next_avail]);
                blk->next_avail += size;
            } else  {
                //FIXME: Perhaps put the salloc free list maintence here?
                //       If the portion allocated is less than the length
                //       of the freed portion, the remainder of it should
                //       be placed back into the free list.
                //       Something like,
                //       newfreed->start = oldfreed->start + size
                //       newfreed->length = oldfreed->length - size
                //       tacked onto the end of the list.
                ptr = &(blk->memory[position]);
            }

            return ptr;
        }

    public:
        MemoryManager() {
            first_block = _block_create();
            block *current = first_block;
            for (int i=0; i<INIT_BLK_CNT; i++) {
                current->next = _block_create();
                current = current->next;
            }
        }

        //void *salloc() {}
        
        void *falloc(int size) 
        {
            if (size > BS) return NULL;

            block *current = first_block;

            while (1) {
                if (block_avail_space(current) > size) { // we can fit it here
                    int avail = sem_trywait(current->locked);
                    if (avail == 0) {
                        void *ptr = allocate(current, size, -1);
                        sem_post(current->locked);
                        return ptr;
                    } 
                }

                // FIXME: Rather than recycling back over the list if
                //        a block could hold the data, but was locked
                //        at the time, this will always create a new
                //        block. Probably worth it to track if a block
                //        "could" work, and then recycle the list if so
                //        rather than spawning tons of new blocks.
                if (!current->next) {
                    current->next = _block_create();
                }

                current = current->next;
            }
        }

        void mmfree(void *free_ptr, int size) 
        {
            char *to_free = (char *) free_ptr;
            block *current;
            int j = 0;
            for (current = first_block; 
                to_free < current->memory || to_free >= (current->memory + BS);
                current = current->next)  {
            }

            if (!current) {
                // tried to free memory not allocated by the manager.
               return;
            }

            int index = (char *) to_free - current->memory;

            //FIXME: Kinda a big critical section here. Might want to see
            //       if I can refactor/design this to be a bit less locky.
            sem_wait(current->locked);
            for (int i=index; i<index+size; i++) {
                current->memory[i] = 0;
            }

            if (current->free_list) { 
                freed *current_free;
                for (current_free = current->free_list; 
                    current_free->next;
                    current_free = current_free->next) ;

                freed *new_free = _create_freed(index, size);
                current_free->next = new_free;
                new_free->prev = current_free;
            } else {
                current->free_list = _create_freed(index, size);
            }

            sem_post(current->locked);
        }

        void dump(FILE *target) 
        {
            block *current;
            int j = 0;
            for (current = first_block; current; current = current->next) {
                fprintf(target, "For block %d:\n", j);
                for (int i=0; i<BS; i++) {
                    fprintf(target, "%5d\t%p\t%10u\n", i, 
                            &(current->memory[i]),
                            (int) current->memory[i]);
                }
                freed *current_free;
                for (current_free=current->free_list; current_free; current_free = current_free->next) {
                    fprintf(target, "(%d, %d) ->", current_free->start, current_free->length);
                }

                fprintf(target, "\n");
                j++;
            }
        }
};


int main()
{
    auto mman = new MemoryManager();
    int **values = (int **) malloc(BS * sizeof(size_t));

    char *test_chr = (char *) mman->falloc(sizeof(char));
    *test_chr = 'A';

    for (int i=0; i<BS; i++) {
        values[i] = (int *) mman->falloc(sizeof(int));
        *(values[i]) = i;
    }

    int *other = (int *) mman->falloc(sizeof(int));
    *other = 45000;


    char *test = (char *) mman->falloc(13 * sizeof(char));
    memcpy(test, "hello, world!", 13);

    int *next_other = (int *) mman->falloc(sizeof(int));
    *next_other = 20;

    char *test3 = (char *) mman->falloc(sizeof(char));
    *test3 = 'h';

    mman->dump(stdout);
    for (int i=0; i<BS; i++) {
        fprintf(stdout, "%d\t%p\t%2d\n", i, values[i], *values[i]);
    }

    mman->mmfree(test, 13*sizeof(char));
    mman->mmfree(test3, sizeof(char));
    mman->mmfree(other, sizeof(int));
    mman->dump(stdout);

    printf("%s\n", test);
}
