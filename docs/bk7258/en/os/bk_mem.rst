Memory Interface
=================================================================

:link_to_translation:`zh_CN:[中文]`



**Memory Management Introduction**
--------------------------------------------------------------------

 - The memory management interface provides a unified interface for dynamic memory allocation and deallocation
 - Supports allocation of two memory types: SRAM and PSRAM
 - Provides auxiliary functions for memory operations (copy, set, compare, etc.)
 - Supports memory debugging functionality to track memory allocation and leaks
 - All memory allocation/deallocation functions **cannot be called in interrupt context**

.. warning::

    - Calling os_malloc/os_free in interrupt context will trigger assertion error
    - Memory allocation may fail and return NULL, must check return value before use
    - Should set pointer to NULL after freeing memory to avoid dangling pointers
    - Frequent small memory allocations may lead to memory fragmentation

**Memory Type Description**
--------------------------------------------------------------------

The Armino platform supports two types of memory:

**SRAM (Static RAM)**
    - On-chip static random access memory, fast access speed
    - Relatively small capacity (typically hundreds of KB)
    - Suitable for storing frequently accessed data and code
    - Relatively low power consumption

**PSRAM (Pseudo Static RAM)**  
    - Off-chip pseudo static random access memory, large capacity
    - Slower access speed compared to SRAM
    - Suitable for storing large data buffers
    - Requires CONFIG_PSRAM_AS_SYS_MEMORY to be enabled

**SRAM Memory Interfaces**
--------------------------------------------------------------------

- Allocate SRAM memory::

    void *os_malloc(size_t size)
    
    Allocates a memory block of specified size from SRAM heap
    
    Parameters:
    -size: Number of bytes to allocate
    
    Return value:
    -Success: Returns pointer to allocated memory
    -Failure: Returns NULL
    
    **FreeRTOS equivalent interface**: pvPortMalloc()
    
.. warning::
        Cannot call this function in interrupt context

- Allocate and zero-initialize SRAM memory::

    void *os_zalloc(size_t size)
    
    Allocates memory from SRAM heap and initializes it to zero
    
    Parameters:
    -size: Number of bytes to allocate
    
    Return value:
    -Success: Returns pointer to allocated memory (content zeroed)
    -Failure: Returns NULL
    
    Implementation: Internally calls os_malloc() then uses os_memset() to zero

- Free SRAM memory::

    void os_free(void *ptr)
    
    Frees memory previously allocated by os_malloc() or os_zalloc()
    
    Parameters:
    -ptr: Pointer to memory to free, can be NULL (safe)
    
    **FreeRTOS equivalent interface**: vPortFree()
    
.. warning::
        Cannot call this function in interrupt context

- Reallocate SRAM memory::

    void *os_realloc(void *ptr, size_t size)
    
    Changes the size of an allocated memory block
    
    Parameters:
    -ptr: Original memory pointer, can be NULL
    -size: New size in bytes
    
    Return value:
    -Success: Returns new memory pointer
    -Failure: Returns NULL, original memory remains unchanged
    
    Implementation: Allocates new memory, copies original data, frees original memory

**PSRAM Memory Interfaces**
--------------------------------------------------------------------

- Allocate PSRAM memory::

    void *psram_malloc(size_t size)
    
    Allocates a memory block of specified size from PSRAM heap
    
    Parameters:
    -size: Number of bytes to allocate
    
    Return value:
    -Success: Returns pointer to allocated memory
    -Failure: Returns NULL
    
    Usage scenarios:
    - Large data buffers (e.g., image, audio/video buffers)
    - Data that doesn't need frequent access
    
.. note::
        Requires CONFIG_PSRAM_AS_SYS_MEMORY to be enabled

- Allocate and zero-initialize PSRAM memory::

    void *psram_zalloc(size_t size)
    
    Allocates memory from PSRAM heap and initializes it to zero
    
    Parameters:
    -size: Number of bytes to allocate
    
    Return value:
    -Success: Returns pointer to allocated memory (content zeroed)
    -Failure: Returns NULL

- Free PSRAM memory::

    void psram_free(void *ptr)
    
    Frees memory previously allocated by psram_malloc() or psram_zalloc()
    
    Parameters:
    -ptr: Pointer to memory to free
    
.. note::
        psram_free is actually a macro definition of os_free, use os_free to free memory uniformly

- Reallocate PSRAM memory::

    void *bk_psram_realloc(void *ptr, size_t size)
    
    Changes the size of an allocated PSRAM memory block
    
    Parameters:
    -ptr: Original PSRAM memory pointer
    -size: New size in bytes
    
    Return value:
    -Success: Returns new memory pointer
    -Failure: Returns NULL

**Memory Operation Interfaces**
--------------------------------------------------------------------

- Memory copy::

    void *os_memcpy(void *out, const void *in, UINT32 n)
    
    Copies n bytes from source address to destination address
    
    Parameters:
    -out: Destination address
    -in: Source address (cannot be NULL)
    -n: Number of bytes to copy
    
    Return value:
    -Returns destination address pointer
    
    **Standard C interface**: memcpy()

- Memory copy (word-aligned optimization)::

    void os_memcpy_word(uint32_t *out, const uint32_t *in, uint32_t n)
    
    Copies memory by word (4 bytes), more efficient than os_memcpy()
    
    Parameters:
    -out: Destination address (uint32_t pointer)
    -in: Source address (uint32_t pointer)
    -n: Number of bytes to copy (4-byte alignment recommended)
    
    Usage scenarios:
    - PSRAM data copy (recommended)
    - Large data transfer

- Memory set::

    void *os_memset(void *b, int c, UINT32 len)
    
    Sets a memory region to a specific value
    
    Parameters:
    -b: Destination address
    -c: Value to set (usually 0)
    -len: Number of bytes to set
    
    Return value:
    -Returns destination address pointer
    
    **Standard C interface**: memset()

- Memory set (word-aligned optimization)::

    void os_memset_word(uint32_t *b, int32_t c, uint32_t n)
    
    Sets memory by word (4 bytes), more efficient than os_memset()
    
    Parameters:
    -b: Destination address (uint32_t pointer)
    -c: Value to set
    -n: Number of bytes to set

- Memory compare::

    INT32 os_memcmp(const void *s1, const void *s2, UINT32 n)
    
    Compares the content of two memory regions
    
    Parameters:
    -s1: First memory address
    -s2: Second memory address
    -n: Number of bytes to compare
    
    Return value:
    -0: Memory contents are identical
    -<0: s1 < s2
    ->0: s1 > s2
    
    **Standard C interface**: memcmp()

- Memory move::

    void *os_memmove(void *out, const void *in, UINT32 n)
    
    Moves n bytes from source to destination, supports overlapping memory
    
    Parameters:
    -out: Destination address
    -in: Source address (cannot be NULL)
    -n: Number of bytes to move
    
    Return value:
    -Returns destination address pointer
    
    **Standard C interface**: memmove()
    
.. note::
        Use os_memmove() instead of os_memcpy() when source and destination overlap

**Memory Debugging Interfaces**
--------------------------------------------------------------------

When CONFIG_MEM_DEBUG or CONFIG_MALLOC_STATIS is enabled, memory allocation functions automatically record the calling location for memory leak detection.

- Debug version memory allocation::

    void *os_malloc_debug(const char *func_name, int line, size_t size, int need_zero)
    
    Parameters:
    -func_name: Calling function name (usually use __FUNCTION__)
    -line: Calling line number (usually use __LINE__)
    -size: Allocation size
    -need_zero: Whether to zero (0: no zero, 1: zero)
    
    Return value:
    -Success: Returns pointer to allocated memory
    -Failure: Returns NULL

- Debug version memory free::

    void *os_free_debug(const char *func_name, int line, void *pv)
    
    Parameters:
    -func_name: Calling function name
    -line: Calling line number
    -pv: Pointer to memory to free
    
    Return value:
    -Returns NULL

- Export memory statistics::

    void os_dump_memory_stats(uint32_t start_tick, uint32_t ticks_since_malloc, const char* task)
    
    Exports memory usage statistics for analyzing memory leaks
    
    Parameters:
    -start_tick: Start tick
    -ticks_since_malloc: Number of ticks since allocation
    -task: Task name filter

**Special Purpose Interfaces**
--------------------------------------------------------------------

- Show memory configuration info::

    void os_show_memory_config_info(void)
    
    Prints current system memory configuration and usage

**Usage Examples**
--------------------------------------------------------------------

Basic memory allocation example::

    #include <os/mem.h>
    
    void memory_basic_example(void)
    {
        void *buffer;
        size_t size = 1024;
        
        // Allocate memory
        buffer = os_malloc(size);
        if (buffer == NULL) {
            os_printf("Failed to allocate memory\r\n");
            return;
        }
        
        // Use memory
        os_memset(buffer, 0, size);
        os_printf("Allocated %d bytes at %p\r\n", size, buffer);
        
        // Free memory
        os_free(buffer);
        buffer = NULL;  // Avoid dangling pointer
    }

Allocate and zero-initialize memory::

    void *buffer = os_zalloc(512);  // Allocate 512 bytes and zero
    if (buffer) {
        // Use buffer
        os_free(buffer);
    }

PSRAM memory allocation example::

    #include <os/mem.h>
    
    void psram_example(void)
    {
        #define LARGE_BUFFER_SIZE  (100 * 1024)  // 100KB
        
        // Allocate large PSRAM memory
        void *large_buffer = psram_malloc(LARGE_BUFFER_SIZE);
        if (large_buffer == NULL) {
            os_printf("Failed to allocate PSRAM\r\n");
            return;
        }
        
        // Use PSRAM buffer
        os_memset(large_buffer, 0, LARGE_BUFFER_SIZE);
        
        // Free PSRAM memory
        psram_free(large_buffer);  // Same as os_free()
    }

Memory reallocation example::

    void realloc_example(void)
    {
        void *buffer;
        void *new_buffer;
        
        // Initially allocate 100 bytes
        buffer = os_malloc(100);
        if (buffer == NULL) return;
        
        // Reallocate to 200 bytes
        new_buffer = os_realloc(buffer, 200);
        if (new_buffer != NULL) {
            buffer = new_buffer;  // Update pointer
        } else {
            // Reallocation failed, original memory still valid
            os_printf("Realloc failed\r\n");
        }
        
        os_free(buffer);
    }

Memory operations example::

    void memory_operations_example(void)
    {
        uint8_t src[100], dst[100];
        
        // Memory set
        os_memset(src, 0xAA, sizeof(src));
        
        // Memory copy
        os_memcpy(dst, src, sizeof(src));
        
        // Memory compare
        if (os_memcmp(src, dst, sizeof(src)) == 0) {
            os_printf("Memory content is same\r\n");
        }
        
        // Memory move (supports overlap)
        os_memmove(src + 10, src, 50);
    }

Structure memory management example::

    typedef struct {
        int id;
        char name[32];
        void *data;
    } my_struct_t;
    
    void struct_memory_example(void)
    {
        my_struct_t *obj;
        
        // Allocate structure memory and zero
        obj = (my_struct_t *)os_zalloc(sizeof(my_struct_t));
        if (obj == NULL) {
            return;
        }
        
        // Initialize structure
        obj->id = 1;
        os_memcpy(obj->name, "test", 5);
        obj->data = os_malloc(256);
        
        // Use structure...
        
        // Free memory
        if (obj->data) {
            os_free(obj->data);
        }
        os_free(obj);
    }

.. note::

    
    - **Memory Deallocation Principles**:
        * Memory allocated with os_malloc should be freed with os_free
        * Memory allocated with psram_malloc should be freed with psram_free (i.e., os_free)
        * Set pointer to NULL immediately after freeing
    
    - **Performance Considerations**:
        * SRAM has fast access speed, suitable for frequent access
        * PSRAM has large capacity but slower access, suitable for large buffers
        * Using os_memcpy_word for word-aligned copy can improve performance
    
    - **Debugging Support**:
        * Enable CONFIG_MEM_DEBUG to track memory allocation locations
        * Enable CONFIG_MALLOC_STATIS to collect memory usage statistics
        * Use memshow command to view memory usage

**Memory Management Macro Definitions**
--------------------------------------------------------------------

The system provides convenient macro definitions for memory debugging::

    // In debug mode, automatically record allocation location
    #if (CONFIG_MALLOC_STATIS || CONFIG_MEM_DEBUG)
    #define os_malloc(size)     os_malloc_debug(__FUNCTION__, __LINE__, size, 0)
    #define os_zalloc(size)     os_malloc_debug(__FUNCTION__, __LINE__, size, 1)
    #define os_free(p)          os_free_debug(__FUNCTION__, __LINE__, p)
    #define psram_malloc(size)  psram_malloc_debug(__FUNCTION__, __LINE__, size, 0)
    #define psram_zalloc(size)  psram_malloc_debug(__FUNCTION__, __LINE__, size, 1)
    #endif
    
    // PSRAM free uses unified os_free
    #define psram_free          os_free

**Reference Information**
--------------------------------------------------------------------

- API header file: ``include/os/mem.h``
- Implementation file: ``components/bk_rtos/freertos/mem_arch.c``


