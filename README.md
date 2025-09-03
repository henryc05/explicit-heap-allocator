**A dynamic memory allocator written in C to replace the standard malloc, realloc and free functions.**

**Properties:**

- Block Placement Policy: first fit placement.

- Management Policy: explicit free list with last-in-first-out policy, as in the most recently freed block becomes the head of the free list.

- Coalescing: immediate coalescing using boundary tags.

- Splitting Blocks: splits blocks which are too large to reduce internal fragmentation.
