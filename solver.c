/* NOTE:
        There are few places in code, where you will see an 'if (0 ...) { }'. Those are for manual branch activation and 
    are left here intentionally, since they are usefull for debugging or activating additional features (like printing
    when 2 paths with same length are found).
*/


#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARENA_IMPLEMENTATION
#include "thirdparty/marena.h"

#define NUMBER_WIDTH 6
#define KEY_WIDTH 2

#define DA_CAPACITY_MUL_FACTOR 2
#define DA_DEFAULT_CAPACITY 128



#define da_init(arr_ptr, new_capacity) \
    do { \
        assert((arr_ptr) != NULL && (new_capacity) > 0); \
        (arr_ptr)->items = arena_alloc(&g_arena, sizeof(*((arr_ptr)->items)) * (new_capacity)); \
        (arr_ptr)->capacity = (new_capacity); \
    } while (0)


#define da_grow(arr_ptr) \
    do { \
        assert((arr_ptr) != NULL); \
        size_t _new_capacity = (arr_ptr)->capacity * DA_CAPACITY_MUL_FACTOR; \
        (arr_ptr)->items = arena_realloc(&g_arena, (arr_ptr)->items, (arr_ptr)->capacity * sizeof(*((arr_ptr)->items)), _new_capacity * sizeof(*((arr_ptr)->items))); \
        (arr_ptr)->capacity = _new_capacity; \
    } while (0)

#define da_remove_ordered(arr_ptr, index) \
    do { \
        assert((arr_ptr) != NULL); \
        assert(index < (arr_ptr)->count); \
 \
        if (index == ((arr_ptr)->count) - 1) { \
            (arr_ptr)->count--; \
        } \
        else { \
            memmove(&((arr_ptr)->items[index]), &((arr_ptr)->items[index + 1]), ((arr_ptr)->count - index - 1) * sizeof(*((arr_ptr)->items))); \
            (arr_ptr)->count--; \
        } \
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
        /* free((arr_ptr)->items); */ /* in case of arena, dont do anything, since there is no way to free specific region */\
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
    Number_ptr_array nodes_in;
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

Arena g_arena = {0};

Number_ptr_array g_longest_sequence = {0};


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
    buckets_table.buckets = arena_alloc_zero(&g_arena, numbers.count * sizeof(Number_ptr_array));
    buckets_table.capacity = numbers.count;

    buckets_fill(&buckets_table, numbers);

    for (size_t i = 0; i < numbers.count; i++) {
        Number* current_number = &(numbers.items[i]);
        Number_ptr_array* key_bucket = &(buckets_table.buckets[current_number->key_end]);
        for (size_t j = 0; j < key_bucket->count; j++) {
            da_append(&(current_number->nodes_out), key_bucket->items[j]);
            da_append(&(key_bucket->items[j]->nodes_in), current_number);
        }
    }
}

Number parse_number(char* num_str, size_t num_str_len)
{
    assert(num_str != NULL);

    char* invalid_char_addr_start = NULL;
    //memset(buff_err, 0, num_str_len);

    long num = strtol(num_str, &invalid_char_addr_start, 10);
    if (invalid_char_addr_start[0] != '\0') {
        fprintf(stderr, "Invalid characters in number '%s'\n", num_str);
        printf("%s\n", invalid_char_addr_start);
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
    char* file_line_buff = arena_alloc(&g_arena, file_line_buff_size);

    ssize_t read_bytes; 
    while ((read_bytes = getline(&file_line_buff, &file_line_buff_size, inputs_file)) > 0) {
        // remove new line, since strtol() treats it as invalid 
        if (file_line_buff[read_bytes - 1] == '\n' || file_line_buff[read_bytes - 1] == '\r') {
            file_line_buff[read_bytes - 1] = '\0';
        }
        //printf("%s\n", file_line_buff);
        //Number number = parse_number(file_line_buff, read_bytes);
        da_append(&number_array, parse_number(file_line_buff, read_bytes));   // TODO: read_bytes should be -1?
    }
    if (!feof(inputs_file)) {
        fprintf(stderr, "Error reading file '%s': %s\n", filename, strerror(errno));
        exit(1);
    }

    fclose(inputs_file);
    return number_array;
}

size_t calculate_len_char(Number_ptr_array sequence)
{
    assert(sequence.items != NULL);
    size_t count = 0;
    for (size_t i = 0; i < sequence.count; i++) {
        if (i == sequence.count - 1) {  // last
            count += NUMBER_WIDTH;
        }
        else {
            count += NUMBER_WIDTH - KEY_WIDTH;
        }
    }
    return count;
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

bool check_sequence(Number_ptr_array number_ptr_array)
{
    for (size_t i = 0; i < number_ptr_array.count; i++) {
        Number* current_elem = number_ptr_array.items[i];
        for (size_t k = 0; k < number_ptr_array.count; k++) {
            if (k != i && number_ptr_array.items[k]->num_full == current_elem->num_full) {
                return false;
            }
        }
    }
    return true;
}

// traverse graph until the last node and than compare sequence against global longest one (g_longest_sequence)
void find_longest_sequence_for_node(Number_array number_array, Number* node, Number_ptr_array* current_sequence) 
{
    da_append(current_sequence, node);
    node->is_used = true;

    size_t available_next_node_count = 0;
    for (size_t i = 0; i < node->nodes_out.count; i++) {
        Number* current_number = node->nodes_out.items[i];
        if (current_number->is_used) {
            continue;
        }
        available_next_node_count++;

        find_longest_sequence_for_node(number_array, current_number, current_sequence);
    }

    // even if there are nodes_out in current number, all of them may already be used, meaning this is an end of sequence  
    if (node->nodes_out.count == 0 || available_next_node_count == 0) {  // tail
        if (0 && current_sequence->count == g_longest_sequence.count && current_sequence->count > 66) {
            printf("Equal: [%ld -> %ld] [%ld -> %ld] len: %zu\n", 
                g_longest_sequence.items[0]->num_full, g_longest_sequence.items[g_longest_sequence.count - 1]->num_full,
                current_sequence->items[0]->num_full, current_sequence->items[current_sequence->count - 1]->num_full,
                current_sequence->count);
        }
        if (current_sequence->count > g_longest_sequence.count) {
            memcpy((g_longest_sequence.items), (current_sequence->items), current_sequence->count * sizeof(*(current_sequence->items)));
            g_longest_sequence.count = current_sequence->count;
        }
    }

    node->is_used = false;
    current_sequence->count--;  // pop each element of sequence
}


void find_longest_sequence(Number_array number_array)
{
    assert(number_array.items != NULL && number_array.count > 0);
    Number_ptr_array current_sequence = {0};    
    da_init(&current_sequence, number_array.count);

    for (size_t i = 0; i < number_array.count; i++) {
        Number* current_number = &(number_array.items[i]);
        if (current_number->nodes_in.count == 0 && current_number->nodes_out.count == 0) {   // skip orphans
            continue;
        }
        current_number->is_used = true;

        printf("[%d]: %ld\n", i, current_number->num_full);

        //da_reset(&current_sequence);    // 'find_longest_sequence_for_node()' reset only to the last added elemend, i.e. it will add nodes on each top-level iteration and NOT remove them
        find_longest_sequence_for_node(number_array, current_number, &current_sequence);

        //print_sequence(longest_for_node);        
        current_number->is_used = false;
    }

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

void print_orphans(Number_array* number_array)
{
    for (size_t i = 0; i < number_array->count; i++) {
        Number* current_number = &(number_array->items[i]);
        if (current_number->nodes_in.count == 0 && current_number->nodes_out.count == 0) {
            printf("(%ld) %ld (%ld)\n", current_number->key_start, current_number->num_full, current_number->key_end);
        }
    }
}

void print_starters(Number_array number_array)
{
    printf("Starters: \n");
    size_t count = 0;
    for (size_t i = 0; i < number_array.count; i++) {
        Number* current_number = &(number_array.items[i]);
        if (current_number->nodes_in.count == 0) {
            printf("(%ld) %ld (%ld)\n", current_number->key_start, current_number->num_full, current_number->key_end);
            count++;
        }
    }
    printf("%zu\n", count);
}


const char* filename = "source.txt";
//const char* filename = "source_small.txt";
int main()
{
    g_arena.page_size = 16 * 1024;


    Number_array number_array = parse_file(filename);
    da_init(&g_longest_sequence, number_array.count);

    nodes_connect_by_key(number_array);

    find_longest_sequence(number_array);

    printf("\nResult: \n");
    print_sequence_pretty(g_longest_sequence);
    //print_sequence(g_longest_sequence);
    printf("Nodes count: %zu\n", g_longest_sequence.count);
    printf("Length in chars: %zu\n", calculate_len_char(g_longest_sequence));

    return 0;
}
