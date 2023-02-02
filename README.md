# manual-string-allocator
Manually requests blocks of memory using mmap to store strings in.
Keeps track of where strings are located and requests more memory when needed. 
Uses linked lists to figure out the next available locations in memory and their sizes.
