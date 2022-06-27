/* stralloc.c --- Bibliothèque d'allocation de chaînes de caractères.  */


#include "stralloc.h"
#include <string.h>
#include<sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
// Idk if we're allowed to modify makefile, so instead of adding -lm, I'll
// implement my own math functions.
// #include <math.h>


// Required: 64-bit system. Tried to make it work on 32-bit, but there was no
// way to test it. So it might work, it might not.

// The amount of allocatable memory is unfortunately limited to 1e158 bytes
// of memory, split between the structs and the data. If you need more, malloc()

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
 *
 * These two blocks will have their own 'headers' to have multiple pages of
 * varying size, ex: 4096 bytes, 8192 bytes, 16384 bytes, etc. The header will
 * be 4096 bytes long, so it can store up to more than 2^512 bytes, in theory.
 */

#define advance_word_size_t(ptr, n) ((ptr) = (size_t *)((size_t *)(ptr) + (n)))


struct String {
    size_t size;
    size_t allocated;
    char *data;
    size_t *handler_data;
    size_t *handler_string;
};

/*
 * handler_handler_string is the pointer to the block of memory pointers
 * that is used to store the String struct.
 * The first size_t is the number of cells in
 * the block. The next floor(number_of_blocks/64) is the of words
 * used to store the flags for the open cells.
 * 1 signifies used, 0 signifies free. After that is where the first cell is
 * they are all aligned to 8 bytes. 64-bit only :)
 */
void *handler_handler_string = NULL;
/*
 * handler_handler_data is the pointer to the block of memory pointers
 * that is used to store the data.
 * It is a more or less free for all area of memory.
 * The first size_t is the pointer to the head of the linked list of free areas.
 * The word it points to is the beginning of the metadata of the cell.
 * The first word contains the pointer to the next area. The next word
 * is the size of the area. If null, there are no more cells available.
 */
void *handler_handler_data = NULL;

/// Returns the index in the word of the first available cell.
/// \param word: The word to search in.
/// \return The index of the first available cell, -1 if none.
size_t first_free_cell(size_t word) {
    for (size_t i = 0; i < sizeof(size_t) * 8; i++) {
        size_t mut_word = word;
        // Goes from left to right, by checking with AND mask.
        // Ex: 11011111 & 00100000 => index of the shift
        if ((mut_word & ((size_t) 1 << (sizeof(size_t) * 8 - i - 1))) ? false
                                                                      : true) {
            return i;
        }
    }

    return -1;
}

/// Ceiling function, rounds to the next highest integer.
/// \param d Double to round.
/// \return d, rounded to the highest size_t integer.
size_t ceil_size_t(double d) {
    return (size_t) (size_t) d + (d - (double) (size_t) d > 0);
}

/// Returns the pointer of the first available cell that matches
/// the requested size in handler_data
/// \param cell The cell to get the pointer for.
/// \return Pointer to the first available cell, NULL if none is available.
char *request_data(String *cell, size_t *handler_data) {
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
            } else {
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
String *request_string(size_t *handler_string) {
    // Go through the handler_string and find the first available cell
    size_t number_of_blocks = *(size_t *) handler_string;

    // Advances to the next word, priming it
    size_t *inspector = handler_string;
    advance_word_size_t(inspector, 1);

    size_t word_offset = 0;
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < (number_of_blocks / sizeof(size_t) * 8); i++) {
        index = first_free_cell(*inspector);
        if (index != -1) {
            found = true;
            // Flip the bit to 1 to signify we're taking it with OR mask
            *inspector |= ((size_t) 1 << (sizeof(size_t) * 8 - index - 1));
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
    size_t *end_of_metadata = handler_string;
    advance_word_size_t(end_of_metadata, 1);
    advance_word_size_t(end_of_metadata,
                        ceil_size_t((double) number_of_blocks /
                        ((double) sizeof(size_t) * 8)));
    String *cell = (String *) end_of_metadata;
    cell += cell_index;
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
void initialize_handler_string(size_t size, size_t *handler_string) {
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

/// Initializes the handler_data so that the first size_t contains the
/// pointer to the the 2nd word, the 2nd word is a pointer to null, and the 3rd
/// is the size of the block minus the first head.
/// \param size Size of the block allocated by mmap
void initialize_handler_data(size_t size, size_t *handler_data) {
    // The first size_t is reserved for the pointer to the first available area.
    size_t *inspector = handler_data;
    *inspector = (size_t) (inspector + 1);
    advance_word_size_t(inspector, 1);
    *inspector = (size_t) NULL;

    advance_word_size_t(inspector, 1);
    size_t available_size = size - sizeof(size_t);
    *inspector = available_size;
}

/// Allocates a new string of size 'size' and
/// returns the pointer to the structure.
/// \param size Size of the memory requested for the string
/// \return Pointer to the string structure
String *str_alloc(size_t size) {
    size_t base_size = sysconf(_SC_PAGESIZE);
    if (handler_handler_string == NULL) {
        handler_handler_string = mmap(NULL, base_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        handler_handler_data = mmap(NULL, base_size,
                                    PROT_READ | PROT_WRITE,
                                    MAP_ANONYMOUS | MAP_PRIVATE,
                                    0, 0);
        // Initialize them both to contain all 0s.
        size_t number_of_blocks = base_size / sizeof(size_t);
        for (size_t i = 0; i < number_of_blocks; i++) {
            *(((size_t *) handler_handler_string) + i) = 0;
            *(((size_t *) handler_handler_data) + i) = 0;
        }

        // A bit of data at the 0th index at least.
        *(size_t *) handler_handler_string = (size_t) mmap(NULL, base_size,
                                                           PROT_READ |
                                                           PROT_WRITE,
                                                           MAP_ANONYMOUS |
                                                           MAP_PRIVATE, 0, 0);
        *(size_t *) handler_handler_data = (size_t) mmap(NULL, base_size,
                                                         PROT_READ | PROT_WRITE,
                                                         MAP_ANONYMOUS |
                                                         MAP_PRIVATE,
                                                         0, 0);

        initialize_handler_string(base_size,
                                  (size_t *) *(size_t *)
                                          handler_handler_string);
        initialize_handler_data(base_size,
                                (size_t *) *(size_t *) handler_handler_data);
    }

    String *cell;
    size_t handler_string_index = 0;
    do {
        size_t *handler_string =
                (size_t *) handler_handler_string + handler_string_index;
        if (*handler_string == 0) {
            // The block has yet to be initialized.
            size_t mmap_size = base_size * power(2, handler_string_index);
            *handler_string =
                    (size_t) mmap(NULL,
                                  mmap_size,
                                  PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE,
                                  0, 0);
            initialize_handler_string(
                    mmap_size,
                    (size_t *) *handler_string);
        }
        cell = request_string((size_t *) *handler_string);
        if (cell == NULL) {
            handler_string_index++;
        }
        if (cell != NULL)
            cell->handler_string = (size_t *) *handler_string;
    } while (cell == NULL);

    cell->size = size;
    cell->allocated = size;

    size_t index = 0;
    while (size > (base_size * power(2, index))) {
        index++;
    }

    do {
        size_t *handler_data =
                (size_t *) handler_handler_data + index;
        if (*handler_data == 0) {
            // Create new block
            size_t mmap_size = base_size * power(2, index);
            *handler_data = (size_t) mmap(NULL,
                                          mmap_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          0, 0);
            initialize_handler_data(mmap_size,
                                    (size_t *) *handler_data);
        }
        char *data = request_data(cell, (size_t *) *handler_data);
        if (data == NULL) {
            index++;
        }
        cell->data = data;
        cell->handler_data = (size_t *) *handler_data;
    } while (cell->data == NULL);
    // Request pointer to the data in the handler_data


    return cell;
}

/// Frees the string structure by assigning the bit in the header to 0
/// \param str The string struct to free
void handler_string_free(const String *str) {
    size_t *handler_string = str->handler_string;
    size_t *inspector = (size_t *) handler_string;
    size_t number_of_blocks = *inspector;
    advance_word_size_t(inspector, ceil_size_t((double) number_of_blocks /
                                               ((double) sizeof(size_t) * 8)));
    advance_word_size_t(inspector, 1);
    String *string_inspector = (String *) inspector;
    size_t index = str - string_inspector;
    size_t word_offset = index / 64;
    size_t bit_offset = index % 64;
    size_t *flag_inspector = ((size_t *) handler_string) + 1 + word_offset;
    // Flips the bit at the bit_offset, indexed 0 at the left.
    *flag_inspector &= ~((size_t) 1 << (sizeof(size_t) * 8 - 1 - bit_offset));
}

// TODO: Make it work

/// Adds free spaces together if they are next to each other in memory.
void handler_data_amalgamate(size_t *handler_data) {
    bool modified = false;
    /*
     * This is the algorithm for amalgamation:
     * 1. Keep previous at the set location, and check is current is next to
     * previous, if so, merge them, then break out of the loop, if not, go to
     * what current is pointing to and repeat until NULL.
     * 2. If none of those worked, move previous to current, and current will
     * once again iterate through all until it's NULL. These two will go until
     * they both hit null, effectively checking all free block next to each
     * other.
     * 3. Keep doing 1 and 2 for every time that in a loop something was merged.
     * 4. If nothing was merged, break out of the loop.
     * While this might seem like a lot of iterating n^2, n shouldn't approach
     * very high values, unless there was some really weird allocating and
     * freeing of memory. Even then, str_compact will fix it.
     */
    do {
        modified = false;
        size_t *prev = (size_t *) handler_data;
        while (prev != NULL) {
            prev = (size_t *) *prev;
            // Starts the iteration of curr at the first free block
            size_t *curr = (size_t *) *(size_t *) handler_data;
            size_t previous_size = *(prev + 1);
            while (curr != NULL) {
                curr = (size_t *) *curr;
                if (curr == prev) continue;

                size_t current_size = *(curr + 1);
                if ((curr - prev) * sizeof(size_t) == previous_size) {
                    // Merge the two blocks
                    *prev = (size_t) *curr;
                    *(prev + 1) = previous_size + current_size;
                    modified = true;
                    break;
                }
            }
            if (modified) break;
        }
    } while (modified);
}

/// Frees the string structure by adding the metadata back to the linked list.
/// \param data Pointer to the start of the data
/// \param allocated Amount of memory that was allocated for the string
void handler_data_free(char *data, size_t allocated, size_t *handler_data) {
    size_t *prev = (size_t *) handler_data;
    size_t *curr = (size_t *) *prev;
    size_t *new_block = (size_t *) data;

    *prev = (size_t) new_block;
    *new_block = (size_t) curr;
    *(new_block + 1) = allocated;

//    handler_data_amalgamate(handler_data);
}

/// Frees the selected string.
/// \param str String to be freed from memory.
void str_free(String *str) {
    if (str == NULL) {
        return;
    }
    handler_data_free(str->data, str->allocated, str->handler_data);

    handler_string_free(str);
}

/// Gets the size of the string
/// \param str String to get the size of
/// \return Size of the string
size_t str_size(String *str) {
    return str->size;
}

/// Gets the pointer of the data in the string
/// \param str String to get the data from
/// \return Pointer to the data in the string
char *str_data(String *str) {
    return str->data;
}

/// Concatenates two strings together and returns the pointer to the new string.
/// \param s1 The first string to concatenate
/// \param s2 The second string to concatenate
/// \return Pointer to the new string
String *str_concat(String *s1, String *s2) {
    size_t s1size = str_size(s1);
    size_t s2size = str_size(s2);
    String *s = str_alloc(s1size + s2size);

    char *sdata = str_data(s);
    memcpy(sdata, str_data(s1), s1size);
    memcpy(sdata + s1size, str_data(s2), s2size);

    return s;
}

/// Compacts the used data memory so that it is de-fragmented.
void str_compact(void) {
    /* Get all strings in handler_string, then get all their data in
     * handler_data, and copy them over to a new mmap area, one after another,
     * which will then compact and de-fragment them.
     */
    // One thing to take into consideration: allocated size can be changed if
    // it fits properly now. Although still have to be at least 2 words.
    void *new_handler_data = mmap(NULL, 4096,
                                  PROT_READ | PROT_WRITE,
                                  MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (new_handler_data == MAP_FAILED) {
        exit(EXIT_FAILURE);
    }


//    munmap(handler_data, 4096)
}

size_t str_get_size(String *string, size_t word, size_t bit) {
    size_t index = word * 64 + bit;
    string += index;
    return string->size;
}

/// Returns the amount of memory used by the strings.
size_t str_livesize(void) {
    // Get the currently used strings in memory from handler_handler_string
    size_t livesize = 0;
    size_t *handler_handler_inspector = (size_t *) handler_handler_string;
    while ((size_t *) *handler_handler_inspector != NULL) {
        size_t *handler_string_inspector =
                (size_t *) *handler_handler_inspector;
        size_t number_of_blocks = *handler_string_inspector;
        size_t word_flags = number_of_blocks / (sizeof(size_t) * 8) + 1;
        advance_word_size_t(handler_string_inspector, 1);
        size_t *beginning_of_strings = handler_string_inspector + word_flags;

        bool finished = false;
        for (size_t word = 0; word < word_flags; word++) {
            for (size_t bit = 0; bit < sizeof(size_t) * 8; bit++) {
                if (word * 64 + bit > number_of_blocks) {
                    finished = true;
                    break;
                }
                size_t mut_word = *handler_string_inspector;
                // Goes from left to right, by checking with AND mask.
                // Ex: 11011111 & 00100000 => index of the shift
                if ((mut_word &
                    ((size_t) 1 << (sizeof(size_t) * 8 - bit - 1)))) {
                    livesize += str_get_size((String *)beginning_of_strings,
                                             word, bit);
                }
            }
            if (finished) break;
            advance_word_size_t(handler_string_inspector, 1);
        }
        advance_word_size_t(handler_handler_inspector, 1);
    }
    return livesize;
}

/// Returns the amount of 'free' memory available.
/// \return Total amount of free memory in bytes.
size_t str_freesize(void) {
    size_t *data_block_inspector = (size_t *) handler_handler_data;
    size_t total_free = 0;
    while ((size_t *) *data_block_inspector != NULL) {
        size_t *data_free_inspector = (size_t *) *data_block_inspector;
        while ((size_t *)*data_free_inspector != NULL) {
            data_free_inspector = (size_t *) *data_free_inspector;
            total_free += *(data_free_inspector + 1);
        }
        advance_word_size_t(data_block_inspector, 1);
    }
    return total_free;
}

/// Returns the total amount of memory used by stralloc.h.
/// \return Total amount of used memory in bytes.
size_t str_usedsize(void) {
    size_t base_size = sysconf(_SC_PAGESIZE);

    // Both headers
    size_t used_size = base_size * 2;

    for (size_t i = 0; i < 2; i++) {
        size_t *block_inspector;
        size_t index = 0;
        if (i == 0) {
            block_inspector = (size_t *) handler_handler_string;
        }
        else {
            block_inspector = (size_t *) handler_handler_data;
        }

        while ((size_t *) *block_inspector != NULL) {
            used_size += base_size * power(2, index);
            index++;
            advance_word_size_t(block_inspector, 1);
        }
    }

    return used_size;
}

