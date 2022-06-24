/* stralloc.c --- Bibliothèque d'allocation de chaînes de caractères.  */

#include "stralloc.h"
#include <string.h>
#include<sys/mman.h>

// Required: 64-bit system. Not our problem if you're archaic. Assumes sizeof(size_t) == 8.
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

#define advance_byte_void(ptr, n) ((ptr) = (void *)((char *)(ptr) + (n)))
#define advance_word_size_t(ptr, n) ((ptr) = (size_t *)((size_t *)(ptr) + (n)))

struct String {
    size_t size;
    char *data;
};

/*
 * handler_string is the pointer to the block of memory that is used to store the String struct.
 * The first size_t is the number of cells in the block. The next floor(number_of_blocks/64) is the of words
 * used to store the flags for the open cells. 1 signifies used, 0 signifies free. After that is where the first cell is
 * they are all aligned to 8 bytes. 64-bit only :)
 */
void* handler_string = NULL;
/*
 * handler_data is the pointer to the block of memory that is used to store the data. It is a more or less free for all
 * area of memory. The first size_t is the pointer to the head of the linked list of free areas. The word it points to
 * is the beginning of the metadata of the cell. The first word contains the size of the cell in bytes. The next word
 * is the pointer to the next cell. If null, there are no more cells available.
 */
void* handler_data = NULL;

/// Returns the index in the word of the first available cell.
/// \param word: The word to search in.
/// \return The index of the first available cell, -1 if none.
size_t first_free_cell(size_t word) {
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        size_t mut_word = word;
        // Goes from left to right, by checking with AND mask. Ex: 11011111 & 00100000 => index of the shift
        if ((mut_word & (1 << (sizeof(size_t) * 8 - i - 1))) ? -1 : i != -1) {
            return i;
        }
    }
    return -1;
}

/// Returns the pointer of the first available cell that matches the requested size in handler_data
/// \param size Size of requested cell
/// \return Pointer to the first available cell, NULL if none is available.
char *request_memory(size_t size) {

}

/// Allocates a new string of size 'size' and returns the pointer to the structure.
/// \param size Size of the memory requested for the string
/// \return Pointer to the string structure
String *str_alloc(size_t size) {
    if (handler_string == NULL) {
        // TODO: Math to get the exact length necessary for x number of string structs aligned with word
        handler_string = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        handler_data = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    }
    // Go through the handler_string and find the first available cell
    size_t number_of_blocks = *(size_t *)handler_string;

    // Advances to the next word, priming it
    size_t *inspector = handler_string;
    advance_word_size_t(inspector, 1);
    size_t word_offset = 0;
    size_t index = 0;
    for (size_t i = 0; i < (number_of_blocks/64); i++) {
        index = first_free_cell(*inspector);
        if (index != -1) {
            // Flip the bit to 1 to signify we're taking it
            *inspector |= (1 << (sizeof(size_t) * 8 - index - 1));
            break;
        }
        advance_word_size_t(inspector, 1);
        word_offset++;
    }
    size_t cell_index = word_offset * sizeof(size_t) * 8 + index;
    // Advance to the first cell_open metadata, then advance num_of_cells/64 to get to the first cell, then move
    // cell_index times sizeof(String) to get to the open cell.
    String *cell = (String *)((size_t *)handler_string + 1 + (number_of_blocks / 64)) + cell_index * sizeof(String);
    cell->size = size;
    // Request pointer to the data in the handler_data


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

