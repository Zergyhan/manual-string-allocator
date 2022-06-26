/* stralloc.c --- Bibliothèque d'allocation de chaînes de caractères.  */

#include "stralloc.h"
#include <string.h>
#include<sys/mman.h>
#include <stdbool.h>
// Idk if we're allowed to modify makefile, so instead of adding -lm, I'll
// implement my own ceil function
// #include <math.h>


// Required: 64-bit system. Not our problem if you're archaic.
// Assumes sizeof(size_t) == 8.
/*
 * Memory will be store in 2 blocks:
 * - The first block will be used to store the String struct,
 * which itself contains a pointer to the data.
 * - The second block will be used to store the data.
 *
 * The first block will be fixed size blocks. The first word (8 bytes)
 * will be used to store the number of cells.
 * The next bits are booleans to indicate if the block is used or not.
 * You can then offset to the first available cell
 * and use it, flipping the bit to 1.
 *
 * The second block will be variable size blocks. The first word will
 * be used to store the pointer to the head of the
 * linked list of free areas. After being pointed, the first word will
 * be the metadata of the cell, indicating the size
 * in bytes. The next word will be the pointer to the next cell. If null,
 * there are no more cells. If trying to add,
 * this will require the creation of a new block of memory
 * and copying the old one over, then freeing the old one.
 */

#define advance_byte_void(ptr, n) ((ptr) = (void *)((char *)(ptr) + (n)))
#define advance_word_size_t(ptr, n) ((ptr) = (size_t *)((size_t *)(ptr) + (n)))

struct String {
    size_t size;
    size_t allocated;
    char *data;
};

/*
 * handler_string is the pointer to the block of memory that is used
 * to store the String struct. The first size_t is the number of cells in
 * the block. The next floor(number_of_blocks/64) is the of words
 * used to store the flags for the open cells.
 * 1 signifies used, 0 signifies free. After that is where the first cell is
 * they are all aligned to 8 bytes. 64-bit only :)
 */
void *handler_string = NULL;
/*
 * handler_data is the pointer to the block of memory that is used
 * to store the data. It is a more or less free for all area of memory.
 * The first size_t is the pointer to the head of the linked list of free areas.
 * The word it points to is the beginning of the metadata of the cell.
 * The first word contains the pointer to the next area. The next word
 * is the size of the area. If null, there are no more cells available.
 */
void *handler_data = NULL;

/// Returns the index in the word of the first available cell.
/// \param word: The word to search in.
/// \return The index of the first available cell, -1 if none.
size_t first_free_cell(size_t word) {
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        size_t mut_word = word;
        // Goes from left to right, by checking with AND mask.
        // Ex: 11011111 & 00100000 => index of the shift
        if ((mut_word & (1 << (sizeof(size_t) * 8 - i - 1))) ? false : true) {
            return i;
        }
    }

    return -1;
}

size_t ceil_size_t(double d) {
    return (size_t) (size_t) d + (d - (size_t) d > 0);
}

/// Returns the pointer of the first available cell that matches
/// the requested size in handler_data
/// \param cell The cell to get the pointer for.
/// \return Pointer to the first available cell, NULL if none is available.
char *request_data(String *cell) {
    // Get the head of the linked list of free areas
    size_t *head = handler_data;
    size_t *prev = head;
    size_t *curr = (size_t *) *head;
    size_t *next = (size_t *) *curr;
    // The size of the area in bytes.
    size_t area_size = *(curr + 1);

    while (curr != NULL) {
        if (area_size >= cell->allocated) {
            // Found a cell that matches the size
            // Allocate the size needed, if the remaining size is
            // 1 word or less word, then just add it to the string
            if (area_size - cell->allocated < 16) {
                cell->allocated = area_size;
            }
            if (cell->allocated == area_size) {
                // Changes linked list so that it is taken out of the list
                *prev = (size_t) next;
            }
            else {
                // Split the cell into two cells, taking the first one,
                // pointing prev to the second one,
                // and pointing the second one to next.
                // The minimum requested is 2 words so that there is always
                // space for the free metadata
                size_t allocated_words = (size_t) (ceil_size_t(
                        (double) cell->allocated / (double) sizeof(size_t)));
                if (allocated_words == 1) {
                    allocated_words++;
                }

                // The location of the second new cell
                size_t *new_cell = (size_t *) curr + allocated_words;

                // Point the new cell to the next cell
                *new_cell = (size_t) next;

                // Set the size of the new cell
                *(new_cell + 1) = area_size - cell->allocated;

                // Point the previous cell to the new cell
                *prev = (size_t) new_cell;
            }
            return (char *) curr;
        }

        prev = curr;
        curr = next;
        if (curr != NULL) {
            area_size = *(curr + 1);
            next = (size_t *) *curr;
        }
    }

    // If all is null, loop finishes and doesn't return what we want,
    // forced to return null
    return NULL;
}

/// Returns the pointer to the first available cell
/// in handler_string for the string struct.
/// \return Pointer to the first String cell
String *request_string() {
    // Go through the handler_string and find the first available cell
    size_t number_of_blocks = *(size_t *) handler_string;

    // Advances to the next word, priming it
    size_t *inspector = handler_string;
    advance_word_size_t(inspector, 1);

    size_t word_offset = 0;
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < (number_of_blocks / 64); i++) {
        index = first_free_cell(*inspector);
        if (index != -1) {
            found = true;
            // Flip the bit to 1 to signify we're taking it with OR mask
            *inspector |= (1 << (sizeof(size_t) * 8 - index - 1));
            break;
        }
        advance_word_size_t(inspector, 1);
        word_offset++;
    }
    if (!found) {
        return NULL;
    }

    size_t cell_index = word_offset * sizeof(size_t) * 8 + index;
    // Advance to the first cell_open metadata,
    // then advance num_of_cells/64 to get to the first cell, then move
    // cell_index times sizeof(String) to get to the open cell.
    String *cell = (String *) ((size_t *) handler_string + 1 +
                               (number_of_blocks / 64)) +
                   (cell_index * sizeof(String));
    return cell;
}

/// Power function without math.h because we idk if we can change CFLAGS :)
/// \param base Exponent base
/// \param exp Exponent
/// \return The result of base^exp
size_t power(size_t base, size_t exp) {
    size_t result = 1;
    for (size_t i = 0; i < exp; i++) {
        result *= base;
    }

    return result;
}

/// Initializes the handler_string so that the first size_t contains
/// the amount of cells available to use and the num_cells/(sizeof(size_t)*8)
/// bits are used for the cell flags.
/// \param size Size of the handler requested by mmap
void initialize_handler_string(size_t size) {
    size_t struct_string_size = sizeof(String);
    // First size_t is reserved for the amount of cells in the block.
    // This is the completely maximum, without any flags.
    size_t max_cells = (size - sizeof(size_t)) / struct_string_size;

    // Now to reserve the flag areas.
    size_t number_of_flag_words = ceil_size_t((double) max_cells / (double) 64);

    // Readjust max_cells, I know there are cases where there might be a word
    // of flags that will just have 1s, but that is not too much of a problem.
    // Ex: 65 blocks of Strings => 2 words of flags, but that extra word takes
    // up the space of the last block, so there's only 64 cells.
    max_cells = (size - (sizeof(size_t)) * (number_of_flag_words + 1)) /
                struct_string_size;
    // Now to set the first size_t to the amount of cells available.
    *(size_t *) handler_string = max_cells;
    // Now to set the rest of the bits to 0.
    size_t *inspector = handler_string;
    advance_word_size_t(inspector, 1);
    for (size_t i = 0; i < number_of_flag_words; i++) {
        *inspector = 0;
        advance_word_size_t(inspector, 1);
    }
    // Change the last flag so that the non-available cells are set to 1.
    inspector = (size_t *) handler_string + 1;
    advance_word_size_t(inspector, number_of_flag_words - 1);

    // Get the max value of size_t, so everything is 1.
    size_t flags = (size_t) -1;
    size_t flags_in_last_block = max_cells % 64;
    for (size_t i = 0; i < flags_in_last_block; i++) {
        flags -= power(2, i);
    }
    *inspector = flags;
}

void initialize_handler_data(size_t size) {
    // The first size_t is reserved for the pointer to the first available area.
    size_t *inspector = handler_data;
    *inspector = (size_t) (inspector + 1);

    advance_word_size_t(inspector, 2);
    size_t available_size = size - sizeof(size_t);
    *inspector = available_size;
}

/// Allocates a new string of size 'size' and
/// returns the pointer to the structure.
/// \param size Size of the memory requested for the string
/// \return Pointer to the string structure
String *str_alloc(size_t size) {
    if (handler_string == NULL) {
        handler_string = mmap(NULL, 4096,
                              PROT_READ | PROT_WRITE,
                              MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        handler_data = mmap(NULL, 4096,
                            PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
                            0, 0);
        initialize_handler_string(4096);
        initialize_handler_data(4096);
    }

    String *cell = request_string();
    if (cell == NULL) {
        return NULL;
    }

    cell->size = size;
    cell->allocated = size;

    // Request pointer to the data in the handler_data
    char *data = request_data(cell);
    if (data == NULL) {
        return NULL;
    }
    cell->data = data;

    return cell;
}

/// Frees the selected string.
/// \param str
void str_free(String *str) {
    return;
}

size_t str_size(String *str) {
    return str->size;
}

char *str_data(String *str) {
    return str->data;
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

