#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARENA_IMPLEMENTATION
#include "marena.h"

/*
    TODO: check whether there are path with equal length but different nodes. What to do in this case?
    - rename 'sibling' to something more apropriate

    FIXME:
        - allocs all of the possible memory, find cause
*/

#define NUMBER_WIDTH 6
#define KEY_WIDTH 2

#define DA_CAPACITY_MUL_FACTOR 2
#define DA_DEFAULT_CAPACITY 128

Arena g_arena = {0};
#define da_init(arr_ptr, new_capacity) \
    do { \
        assert((arr_ptr) != NULL && (new_capacity) > 0); \
        (arr_ptr)->items = malloc(sizeof(*((arr_ptr)->items)) * (new_capacity)); \
        /* (arr_ptr)->items = arena_alloc(&g_arena, sizeof(*((arr_ptr)->items)) * (new_capacity)); */\
        assert((arr_ptr)->items != NULL && "malloc() failed"); \
        (arr_ptr)->capacity = (new_capacity); \
    } while (0)


#define da_grow(arr_ptr) \
    do { \
        assert((arr_ptr) != NULL); \
        size_t _new_capacity = (arr_ptr)->capacity * DA_CAPACITY_MUL_FACTOR; \
        (arr_ptr)->items = realloc((arr_ptr)->items, _new_capacity * sizeof(*((arr_ptr)->items))); \
        /*(arr_ptr)->items = arena_alloc(&g_arena, _new_capacity * sizeof(*((arr_ptr)->items))); */\
        assert((arr_ptr)->items != NULL && "realloc() failed"); \
        (arr_ptr)->capacity = _new_capacity; \
    } while (0)


#define da_append(arr_ptr, elem) \
    do { \
        if ((arr_ptr)->items == NULL || (arr_ptr)->capacity == 0) { \
            da_init((arr_ptr), DA_DEFAULT_CAPACITY); \
        } \
        if ((arr_ptr)->count >= (arr_ptr)->capacity) { \
            da_grow((arr_ptr)); \
        } \
        (arr_ptr)->items[(arr_ptr)->count++] = (elem); \
    } while (0) 

#define da_reset(arr_ptr) \
    do { \
        assert((arr_ptr) != NULL); \
        (arr_ptr)->count = 0; \
    } while (0)

#define da_free(arr_ptr) \
    do { \
        assert((arr_ptr) != NULL); \
        assert((arr_ptr)->items != NULL); \
        free((arr_ptr)->items); \
    } while (0)

#define da_cpy(da_dst, da_src)  \
    do { \
        assert((da_dst) != NULL); \
        assert((da_src) != NULL); \
        for (size_t i = 0; i < (da_src)->count; i++) { \
            da_append((da_dst), (da_src)->items[i]); \
        } \
    } while (0)
    

typedef struct {
    long start_key;
    long end_key;
    long num_full;
    bool is_used;
    //Number_array nodes_in;
    //Number_array nodes_out;
} Number;

typedef struct {
    Number* items;
    size_t capacity;
    size_t count;
} Number_array;

typedef struct {
    Number** items;
    size_t count;
    size_t capacity;
} Number_ptr_array;

Number parse_number(char* num_str, size_t num_str_len)
{
    assert(num_str != NULL);
    char* buff_err[num_str_len] = {}; 
    //memset(buff_err, 0, num_str_len);

    long num = strtol(num_str, buff_err, 10);
    if (*buff_err[0] != '\0') {
        fprintf(stderr, "Invalid characters in number '%s'\n", num_str);
        printf("%s\n", *buff_err);
        printf("^ starting from\n");
        exit(1);
    }

    long start_key = num / 10000;
    long end_key = num % 100;
    return (Number) {
        .start_key = start_key,
        .end_key = end_key,
        .num_full = num,
    };
}

#define BUFFER_SIZE 1024
Number_array parse_file(const char* filename)
{
    assert(filename != NULL);
    
    FILE* inputs_file = fopen(filename, "r");
    if (inputs_file == NULL) {
        fprintf(stderr, "Failed to open file '%s': %s\n", filename, strerror(errno));
        exit(1);
    }

    Number_array number_array = {0};

    size_t file_line_buff_size = BUFFER_SIZE;
    char* file_line_buff = malloc(file_line_buff_size);
    assert(file_line_buff != NULL && "malloc() failed");


    ssize_t read_bytes; 
    while ((read_bytes = getline(&file_line_buff, &file_line_buff_size, inputs_file)) > 0) {
        // remove new line, since strtol() treats it as invalid 
        if (file_line_buff[read_bytes - 1] == '\n') {
            file_line_buff[read_bytes - 1] = '\0';
        }
        //printf("%s\n", file_line_buff);
        da_append(&number_array, parse_number(file_line_buff, read_bytes));   // TODO: read_bytes should be -1?
    }
    if (!feof(inputs_file)) {
        fprintf(stderr, "Error reading file '%s': %s\n", filename, strerror(errno));
        exit(1);
    }

    fclose(inputs_file);
    return number_array;
}

void print_sequence_pretty(Number_ptr_array sequence)
{
    assert(sequence.items != NULL);
    for (size_t i = 0; i < sequence.count; i++) {
        Number* current_number = sequence.items[i];
        long num_no_end_key = current_number->num_full / 100;
        if (i == sequence.count - 1) {  // last
            printf("%0.*ld\n", NUMBER_WIDTH, current_number->num_full);
        }
        else {
            printf("%0.*ld", NUMBER_WIDTH - KEY_WIDTH, num_no_end_key);
        }
    }
}

void print_sequence(Number_ptr_array sequence)
{
    //assert(sequence.items != NULL);
    for (size_t i = 0; i < sequence.count; i++) {
        Number* current_number = sequence.items[i];
        printf("(%ld) %ld (%ld) --> ", current_number->start_key, current_number->num_full, current_number->end_key);
    }
    printf("\n");
}

Number_ptr_array find_all_siblings(Number_array number_array, Number* num)
{
    assert(number_array.items != NULL);
    assert(num != NULL);

    Number_ptr_array number_ptr_array = {0}; 
    for (size_t i = 0; i < number_array.count; i++) {
        Number* current_number = &(number_array.items[i]);
        if (current_number->num_full != num->num_full && !(current_number->is_used) && num->end_key == current_number->start_key) {
            da_append(&number_ptr_array, current_number);
        }
    }
    return number_ptr_array;
}

Number_ptr_array find_longest_sequence_for_node(Number_array number_array, Number* node, Number_ptr_array* current_sequence, size_t depth) 
{
    da_append(current_sequence, node);
    node->is_used = true;

    //Number_ptr_array* local_longest = arena_alloc(&g_arena, sizeof(Number_ptr_array));
    //memset(local_longest, 0, sizeof(*local_longest));
    
    Number_ptr_array local_longest = {0};

    //Number_ptr_array number_ptr_array = find_all_siblings(number_array, node);

    //for (size_t i = 0, count = 0; i < number_ptr_array.count; i++) {
    size_t count = 0;
    for (size_t i = 0; i < number_array.count; i++) {
        Number* current_number = &(number_array.items[i]);
        if (0 && current_number->is_used) {
            printf("%ld in use, depth: %zu\n", current_number->num_full, depth);
        }
        if (current_number->num_full != node->num_full && !(current_number->is_used) && node->end_key == current_number->start_key) {
            ++count;
            current_sequence->count = depth + 1;    // reset array for each sibling 

            Number_ptr_array longest_for_next_node = find_longest_sequence_for_node(number_array, current_number, current_sequence, depth + 1);
            if (0 && longest_for_next_node.count == local_longest.count && longest_for_next_node.count >= 66) {
                printf("------------------------------\n");
                printf("Equal size paths: %zu\n", longest_for_next_node.count);   // TODO: print paths
                //print_sequence_pretty(longest_for_next_node);
                //print_sequence_pretty(local_longest);
                printf("------------------------------\n");
            }
            if (longest_for_next_node.count > local_longest.count) {
                if (local_longest.items != NULL) da_free(&local_longest);
                local_longest = longest_for_next_node;
            }
            else {
                da_free(&longest_for_next_node);
            }
        }
    }

    if (count == 0) {  // tail
        da_cpy(&local_longest, current_sequence);
        goto ret;
    }

ret:
    node->is_used = false;
    return local_longest;
}


// TODO: free arrays if they are not longer then current
Number_ptr_array find_longest_sequence(Number_array number_array)
{
    assert(number_array.items != NULL && number_array.count > 0);
    Number_ptr_array current_sequence = {0};    
    da_init(&current_sequence, number_array.count);

    Number_ptr_array longest_sequence = {0};    
    da_init(&longest_sequence, number_array.count);

    //Arena_mark mark_start_iter = arena_mark(&g_arena); 

    for (size_t i = 0; i < number_array.count; i++) {
        Number* current_number = &(number_array.items[i]);
        current_number->is_used = true;

        printf("[%d]: %ld\n", i, current_number->num_full);

        da_reset(&current_sequence);    // 'find_longest_sequence_for_node()' reset only to the last added elemend, i.e. it will add nodes on each top-level iteration and NOT remove them
        Number_ptr_array longest_for_node = find_longest_sequence_for_node(number_array, current_number, &current_sequence, 0);

            if (longest_for_node.count == longest_sequence.count && longest_for_node.count >= 0) {
                printf("------------------------------\n");
                printf("Equal size paths: %zu\n", longest_for_node.count);   // TODO: print paths
                //print_sequence_pretty(longest_for_next_node);
                //print_sequence_pretty(local_longest);
                printf("------------------------------\n");
            }
        if (longest_for_node.count > longest_sequence.count) {
            longest_sequence = longest_for_node;
        }
        //arena_restore(&g_arena, &mark_start_iter);
        //print_sequence(longest_for_node);        
        current_number->is_used = false;
    }
    return longest_sequence;
}

const char* filename = "source.txt";
//const char* filename = "source_small.txt";
int main()
{
    Number_array number_array = parse_file(filename);
    Number_ptr_array longest_sequence = find_longest_sequence(number_array);
    print_sequence_pretty(longest_sequence);
    print_sequence(longest_sequence);
    printf("len: %zu\n", longest_sequence.count);
    //for (size_t i = 0; i < number_array.count; i++) {
    //    Number* current_number = &(number_array.items[i]);
    //    printf("(%ld) %ld (%ld)\n", current_number->start_key, current_number->num_full, current_number->end_key);
    //}
    return 0;
}
