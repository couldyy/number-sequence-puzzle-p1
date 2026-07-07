#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARENA_IMPLEMENTATION
#include "thirdparty/marena.h"

/*
    TODO: check whether there are path with equal length but different nodes. What to do in this case?
    - rename 'sibling' to something more apropriate

    FIXME:
        [*] - allocs all of the possible memory, find cause
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
    

typedef struct Number Number;

typedef struct {
    Number** items;
    size_t count;
    size_t capacity;
} Number_ptr_array;

typedef struct Number {
    long key_start;
    long key_end;
    long num_full;
    bool is_used;
    //Number_array nodes_in;
    Number_ptr_array nodes_out;
} Number;

typedef struct {
    Number* items;
    size_t capacity;
    size_t count;
} Number_array;

typedef struct {
    size_t count;
    size_t capacity;
    Number_ptr_array* buckets;
} Buckets_table;


void buckets_free(Buckets_table* buckets_table)
{
    assert(buckets_table != NULL);
    for (size_t i = 0; i < buckets_table->capacity; i++) {
        Number_ptr_array* current_bucket = &(buckets_table->buckets[i]);
        if (current_bucket->items != NULL) {
            da_free(current_bucket);
        }
    }
    free(buckets_table->buckets);
}


void buckets_fill(Buckets_table* key_buckets, Number_array numbers)
{
    assert(key_buckets != NULL);
    assert(numbers.items != NULL);
    assert(key_buckets->capacity >= numbers.count);
    
    for (size_t i = 0; i < numbers.count; i++) {
        Number* current_number = &(numbers.items[i]);
        size_t index = current_number->key_start;   // index must be < bucket capacity, but it's fine, since it is checked via assert() previously
        Number_ptr_array* bucket = &(key_buckets->buckets[index]);
        da_append(bucket, current_number);
    }
}

void nodes_connect_by_key(Number_array numbers)
{
    assert(numbers.items != NULL); 

    Buckets_table buckets_table = {0};
    buckets_table.buckets = calloc(numbers.count, sizeof(Number_ptr_array));
    assert(buckets_table.buckets != NULL && "calloc() failed");
    buckets_table.capacity = numbers.count;

    buckets_fill(&buckets_table, numbers);

    for (size_t i = 0; i < numbers.count; i++) {
        Number* current_number = &(numbers.items[i]);
        Number_ptr_array* key_bucket = &(buckets_table.buckets[current_number->key_end]);
        for (size_t j = 0; j < key_bucket->count; j++) {
            da_append(&(current_number->nodes_out), key_bucket->items[j]);
        }
    }
   
    buckets_free(&buckets_table);
}

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

    long key_start = num / 10000;
    long key_end = num % 100;
    Number number = {0};
    number.key_start = key_start;
    number.key_end = key_end;
    number.num_full = num;
    return number;
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
        long num_no_key_end = current_number->num_full / 100;
        if (i == sequence.count - 1) {  // last
            printf("%0.*ld\n", NUMBER_WIDTH, current_number->num_full);
        }
        else {
            printf("%0.*ld", NUMBER_WIDTH - KEY_WIDTH, num_no_key_end);
        }
    }
}

void print_sequence(Number_ptr_array sequence)
{
    //assert(sequence.items != NULL);
    for (size_t i = 0; i < sequence.count; i++) {
        Number* current_number = sequence.items[i];
        printf("(%ld) %ld (%ld) --> ", current_number->key_start, current_number->num_full, current_number->key_end);
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
        if (current_number->num_full != num->num_full && !(current_number->is_used) && num->key_end == current_number->key_start) {
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



    size_t available_next_node_count = 0;
    for (size_t i = 0; i < node->nodes_out.count; i++) {
        Number* current_number = node->nodes_out.items[i];
        if (current_number->is_used) {
            //printf("%ld in use, depth: %zu\n", current_number->num_full, depth);
            continue;
        }
        current_sequence->count = depth + 1;    // reset array for each sibling 
        available_next_node_count++;

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

    // even if there are nodes_out in current number, all of them may already be used, meaning this is an end of sequence  
    if (node->nodes_out.count == 0 || available_next_node_count == 0) {  // tail
        da_cpy(&local_longest, current_sequence);
        goto ret;
    }

ret:
    node->is_used = false;
    return local_longest;
}


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

            if (0 && longest_for_node.count == longest_sequence.count && longest_for_node.count >= 0) {
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

void print_children(Number_array number_array)
{
    Number_ptr_array nodes_out_linear = {0};
    for (size_t i = 0; i < number_array.count; i++) {
        da_reset(&nodes_out_linear);

        printf("------------------------------\n");
        Number* current_number = &(number_array.items[i]);
        printf("%ld (%ld) |\n", current_number->num_full, current_number->key_end);
        for (size_t j = 0; j < current_number->nodes_out.count; j++) {
            printf("\t > %ld\n", current_number->nodes_out.items[j]->num_full);
        }

        // check whether it is the same as finding linearly
        if (0) {
            // Fill linearly
            for (size_t j = 0; j < number_array.count; j++) {
                Number* current_node = &(number_array.items[j]);
                if (current_node->num_full != current_number->num_full && current_number->key_end == current_node->key_start) {
                    da_append(&nodes_out_linear, current_node);
                }
            }
            printf("++++++++++\n");
            printf("%ld (%ld) |\n", current_number->num_full, current_number->key_end);
            for (size_t j = 0; j < nodes_out_linear.count; j++) {
                printf("\t > %ld ", nodes_out_linear.items[j]->num_full);
                if (nodes_out_linear.items[j] != current_number->nodes_out.items[j]) {
                    printf(" (DONT MATCH)");
                }
                printf("\n");
            }
        }
    }
}
const char* filename = "source.txt";
//const char* filename = "source_small.txt";
int main()
{
    Number_array number_array = parse_file(filename);
    nodes_connect_by_key(number_array);

    Number_ptr_array longest_sequence = find_longest_sequence(number_array);
    print_sequence_pretty(longest_sequence);
    print_sequence(longest_sequence);
    printf("len: %zu\n", longest_sequence.count);
    //for (size_t i = 0; i < number_array.count; i++) {
    //    Number* current_number = &(number_array.items[i]);
    //    printf("(%ld) %ld (%ld)\n", current_number->key_start, current_number->num_full, current_number->key_end);
    //}
    return 0;
}
