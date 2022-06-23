/* stralloc.c --- Bibliothèque d'allocation de chaînes de caractères.  */

#include "stralloc.h"
#include <string.h>

/*
 * Memory will be store in 2 blocks:
 * - The first block will be used to store the String struct, which itself contains a pointer to the data.
 * - The second block will be used to store the data.
 *
 * The first block will be fixed size blocks. The first word (8 bytes) will be used to store the number of cells.
 * The next bits are booleans to indicate if the block is used or not. You can then offset to the first available cell
 * and use it, flipping the bit to 1.
 *
 * The second block will be variable size blocks. The first word will be used to store the pointer to the head of the
 * linked list of free areas. After being pointed, the first word will be the metadata of the cell, indicating the size
 * in bytes. The next word will be the pointer to the next cell. If null, there are no more cells. If trying to add,
 * this will require the creation of a new block of memory and copying the old one over, then freeing the old one.
 */

struct String {
    size_t size;
    char *data;
};

// str_alloc without malloc, using mmap and munmap
String *str_alloc(size_t size) {

}

size_t str_size(String *str) {
    return str->size;
}

char *str_data(String *str) {
    return str->data;
}

void str_free(String *str) {
}

String *str_concat(String *s1, String *s2) {
    size_t s1size = str_size(s1);
    size_t s2size = str_size(s2);
    String *s = str_alloc(s1size + s2size);
    char *sdata = str_data(s);
    memcpy(sdata, str_data(s1), s1size);
    memcpy(sdata + s1size, str_data(s2), s2size);
    return s;
}

void str_compact(void) {
    /* Not implemented.  */
}

size_t str_livesize(void) {
    /* Not implemented.  */
    return 0;
}

size_t str_freesize(void) {
    /* Not implemented.  */
    return 0;
}

size_t str_usedsize(void) {
    /* Not implemented.  */
    return 0;
}

