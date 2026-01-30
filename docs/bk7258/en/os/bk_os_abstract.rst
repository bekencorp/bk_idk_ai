OS Abstraction Layer
=================================================================

:link_to_translation:`zh_CN:[中文]`



**OS Abstract Layer Introduction**
--------------------------------------------------------------------

 - The OS abstraction layer is mainly used to adapt to different operating systems of the Armino platform
 - For different operating systems, the OS abstraction layer provides a unified set of interfaces
 - At present, the operating systems supported by the OS abstraction layer of the Armino platform is FreeRTOS
 - At present, the posix interface of the Armino platform only supports the FreeRTOS V10 operating system, which is turned off by default. 
   If it is used, you need to turn on the 'CONFIG_FREERTOS_POSIX' configuration switch
 - API header file: ``include/os/os.h``
 - Implementation source file: ``components/bk_rtos/freertos/v10/rtos_pub.c``

.. note::

 - When using the posix function of FreeRTOS,you need to reference the FreeRTOS_POSIX.h header file before referencing other related header.
 - You can sustomize the relevant configuration in the components/bk_rtos/freertos/posix/freertos_impl/include/portable/bk/FreeRTOS_POSIX_portable.h,
   for example,if you want to use custom or compiler-proviede functions or data structures,you can block posix related functions in this file.
 - When porting posix,if you encounter conflicts with the compiler's own header files,please check the above points first.

**Key Differences Between Abstraction Layer and Native FreeRTOS**
--------------------------------------------------------------------

To provide a unified operating system interface, the abstraction layer differs from native FreeRTOS in several aspects:

**Priority Inversion**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In native FreeRTOS, higher numerical values represent higher priorities. However, in the OS abstraction layer, the opposite rule is adopted for unified management:

 - **Abstraction Layer Priority**: Lower numbers mean higher priority (0 is highest, 9 is lowest)
 - **Native FreeRTOS Priority**: Higher numbers mean higher priority
 - **Conversion Formula**: ``Native Priority = RTOS_HIGHEST_PRIORITY - Abstraction Layer Priority``

For example: When you set priority to 7 in the abstraction layer, the actual FreeRTOS native priority will be automatically converted based on the system's maximum priority configuration.

**Unified Handle Types**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The abstraction layer uses void pointer types for unified handles, hiding the underlying implementation differences of different operating systems:

 - ``beken_thread_t``: Task handle
 - ``beken_queue_t``: Queue handle
 - ``beken_semaphore_t``: Semaphore handle
 - ``beken_mutex_t``: Mutex handle
 - ``beken_timer_t``: Timer handle

**Unified Error Codes**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The abstraction layer defines unified error codes instead of directly using FreeRTOS return values like pdPASS/pdFAIL:

 - ``kNoErr``: Operation successful
 - ``kGeneralErr``: General error
 - ``kTimeoutErr``: Timeout error
 - ``kUnsupportedErr``: Unsupported operation

**Unified Timeout Parameters**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The abstraction layer uses milliseconds as the timeout unit and provides unified macro definitions:

 - ``BEKEN_WAIT_FOREVER`` (0xFFFFFFFF): Wait forever
 - ``BEKEN_NO_WAIT`` (0): No wait
 - Other values: Timeout in milliseconds

While FreeRTOS native interfaces use system clock ticks as the unit, the abstraction layer automatically performs the conversion.

**Task Name Handling**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The abstraction layer differs from native FreeRTOS in how task names are handled:

 - **Abstraction Layer**: Uses pointer passing, directly saves the passed task name pointer without copying the string content
 - **Native FreeRTOS**: Uses copying, creates an internal copy of the task name string

.. important::

    - This means when creating tasks, the passed task name string must remain valid (e.g., use string constants), and cannot use temporary variables on the stack.

**Abstraction Layer API**
---------------------------


**Task Creation:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Create a thread in SRAM::
  
    bk_err_t rtos_create_sram_thread( beken_thread_t* thread, 
                                    uint8_t priority, 
                                    const char* name,
                                    beken_thread_function_t function, 
                                    uint32_t stack_size, 
                                    beken_thread_arg_t arg )

- Create a thread in PSRAM::

    bk_err_t rtos_create_psram_thread( beken_thread_t* thread, 
                                     uint8_t priority,
                                     const char* name,
                                     beken_thread_function_t function, 
                                     uint32_t stack_size, 
                                     beken_thread_arg_t arg )

- Create a task::

    bk_err_t rtos_create_thread( beken_thread_t* thread, 
                                uint8_t priority, 
                                const char* name,
                                beken_thread_function_t function, 
                                uint32_t stack_size, 
                                beken_thread_arg_t arg )

    Parameters:
    -thread: Pointer to the task handle. If NULL is passed, the task handle is not saved
    -priority: Task priority, valid range 0-9
    Note: For the freertos operating system kernel, the higher the priority number of the task, the higher the actual priority.
    For the application layer, the opposite is true. The higher the priority number configured when creating a task, the lower the actual priority.
    This is because in the SDK, the operating system adaptation layer makes a unified management. So the maximum priority seen by the user is 0, and the minimum priority is 9.
        
        Priority table:
            * 0: Highest priority (system reserved)
            * 1-5: Library tasks and SDK internal tasks
            * 6-8: Application tasks (recommended range)
            * 9: Lowest priority (near idle task)
        
        Recommended values:
            * BEKEN_APPLICATION_PRIORITY (7): Normal application tasks
            * BEKEN_DEFAULT_WORKER_PRIORITY (6): Worker tasks
    -name: Task name string
    -function: Task entry function pointer, prototype: void function(beken_thread_arg_t arg)
    -stack_size: Task stack size in bytes. Recommended values:
        * 0x400 (1024 bytes): Simple tasks
        * 0x800 (2048 bytes): General tasks
        * 0x1000 (4096 bytes): Complex tasks
    -arg: Parameter passed to the task function, can be NULL
    
    Return values:
    -kNoErr: Task creation successful
    -kGeneralErr: Task creation failed

    This function internally distinguishes based on macros. If both CONFIG_TASK_STACK_IN_PSRAM and 
    CONFIG_PSRAM_AS_SYS_MEMORY are enabled, it calls rtos_create_psram_thread; otherwise, it calls 
    rtos_create_sram_thread.
    
    **FreeRTOS equivalent interface**: xTaskCreate()

    Usage example::

        #include <os/os.h>

        beken_thread_t task_handle = NULL;

        void my_task_entry(beken_thread_arg_t arg)
        {
            int count = 0;
            while(1) {
                BK_LOGI(NULL, "Task running, count=%d\r\n", count++);
                rtos_delay_milliseconds(1000);  // Delay 1 second
            }
        }

        void create_task_example(void)
        {
            bk_err_t ret;
              
            ret = rtos_create_thread(&task_handle,
                                    BEKEN_APPLICATION_PRIORITY,
                                    "my_task",
                                    my_task_entry,
                                    0x800,  // 2KB stack space
                                    NULL);
            
            if (ret != kNoErr) {
                BK_LOGI(NULL, "Failed to create task\r\n");
            }
        }

.. note::

    - Tasks are created in ready state and wait for scheduler to run them
    - Task functions should generally be designed as infinite loops, and call rtos_delete_thread() if they need to exit
    - It is recommended to use delay functions in tasks to avoid occupying CPU for long periods
    - Insufficient stack space will cause task runtime exceptions, need to set appropriate stack size according to actual usage

**Task Suspension and Recovery:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Suspend a task::

    void rtos_suspend_thread(beken_thread_t* thread)

    Parameters:
    -thread：the handle of the task, if the parameter is NULL, it means to suspend or resume itself

- Suspend the scheduler::

    void rtos_suspend_all_thread(void)

- Resume a task::

    void rtos_resume_thread(beken_thread_t* thread)

- Resume the scheduler::

    void rtos_resume_all_thread(void)

**Other Task Related Interfaces:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
In addition to encapsulating the underlying standard task interfaces, the adaptation layer also provides the following task-related interfaces.

- Delete a task::

    bk_err_t rtos_delete_thread( beken_thread_t* thread )

    Parameters:
    -thread: Task handle pointer to delete. If NULL is passed, deletes the current task itself
    
    Return value:
    -kNoErr: Task deletion successful
    -kGeneralErr: Task deletion failed
    
    **FreeRTOS equivalent interface**: vTaskDelete()
    
    Usage example::
    
        // Delete a specific task
        rtos_delete_thread(&task_handle);
        
        // Task self-deletion
        void task_entry(beken_thread_arg_t arg)
        {
            // Execute task logic
            BK_LOGI(NULL, "Task complete\r\n");
            
            // Self-delete after task completion
            rtos_delete_thread(NULL);
        }

- Check if the specified thread is the currently running thread::

    bool rtos_is_current_thread(beken_thread_t *thread)

    Parameters:
    -thread: Specified task handle pointer

    Return value:
    -true: Is the current task
    -false: Is not the current task
    
    **FreeRTOS equivalent interface**: xTaskGetCurrentTaskHandle()

- Get the handle of the currently running task::

    beken_thread_t* rtos_get_current_thread(void)

    Return value:
    -Returns the current task handle pointer
    
    **FreeRTOS equivalent interface**: xTaskGetCurrentTaskHandle()

- Set the priority of the specified task::

    bk_err_t rtos_thread_set_priority(
                beken_thread_t *thread, 
                int priority)
    
    Parameters:
    -thread: Task handle pointer to set priority for
    -priority: New priority, valid range 0-9
    
    Return value:
    -kNoErr: Set successful
    -kGeneralErr: Set failed
    
    **FreeRTOS equivalent interface**: vTaskPrioritySet()

- Suspend the specified task for a specific time, in seconds::

    void rtos_thread_sleep(uint32_t seconds)
    
    Parameters:
    -seconds: Sleep time in seconds
    
    **FreeRTOS equivalent interface**: vTaskDelay()

- Suspend the specified task for a specific time, in milliseconds::

    bk_err_t rtos_delay_milliseconds(uint32_t num_ms )
    
    Parameters:
    -num_ms: Delay time in milliseconds
    
    Return value:
    -kNoErr: Delay successful
    
    **FreeRTOS equivalent interface**: vTaskDelay()
    
    Usage example::
    
        void periodic_task(beken_thread_arg_t arg)
        {
            while(1) {
                // Execute periodic task
                process_data();
                
                // Delay 100 milliseconds
                rtos_delay_milliseconds(100);
            }
        }

.. note::

    - Task delay will trigger system scheduling, yielding CPU to other tasks
    - Delay accuracy is related to system clock tick, default is 1ms
    - Actual delay time may be slightly longer than set value, this is normal

**Queue:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Initialize a queue::

    bk_err_t rtos_init_queue( beken_queue_t* queue, 
                            const char* name,
                            uint32_t message_size, 
                            uint32_t number_of_messages )
    Parameters:
    -queue：the handle of the queue
    -name: the name of the queue
    -message_size: the size of the message in the queue
    -number_of_messages：the total length of the message (equivalent to the depth of the queue)

    Return value:
    -kNoErr：queue operation successful
    -kGeneralErr: queue operation failed


- Send a message to the tail of the queue::    

    bk_err_t rtos_push_to_queue( beken_queue_t* queue, 
                                 void* message,
                                 uint32_t timeout_ms )
    
    It will determine whether the current is in the interrupt context or the task context, and then call the different kernel interfaces to implement the corresponding functions

    Parameters:
    -timeout_ms：the timeout time, can be configured as
        0: no timeout
        BEKEN_WAIT_FOREVER: block until timeout
        other values: the timeout time

- Send a message to the head of the queue::

    bk_err_t rtos_push_to_queue_front( beken_queue_t* queue,   
                                       void* message, 
                                       uint32_t timeout_ms )

    It will determine whether the current is in the interrupt context or the task context, and then call the different kernel interfaces to implement the corresponding functions

- Receive a message from the queue::

    bk_err_t rtos_pop_from_queue( beken_queue_t* queue, 
                                  void* message, 
                                  uint32_t timeout_ms )
    
    It calls xQueueReceive, only supports in the task context

- Delete a queue::

    bk_err_t rtos_deinit_queue( beken_queue_t* queue )

- Check if a queue is empty::

    bool rtos_is_queue_empty( beken_queue_t* queue )

    Only can be called in the interrupt context

- Check if a queue is full::

    bool rtos_is_queue_full( beken_queue_t* queue )

    Only can be called in the interrupt context
    
    Parameters:
    -queue: Queue handle pointer
    
    Return value:
    -true: Queue is full
    -false: Queue is not full
    
    **FreeRTOS equivalent interface**: xQueueIsQueueFullFromISR()

Complete usage example (refer to components/demos/os/os_queue/os_queue.c)::

    #include <os/os.h>

    typedef struct {
        int value;
    } msg_t;

    static beken_queue_t os_queue = NULL;

    // Receiver task
    void receiver_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        msg_t received = {0};
        
        while(1) {
            // Receive message from queue, wait forever
            err = rtos_pop_from_queue(&os_queue, &received, BEKEN_WAIT_FOREVER);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Received: value=%d\r\n", received.value);
            }
        }
    }

    // Sender task
    void sender_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        msg_t my_message = {0};
        
        while(1) {
            my_message.value++;
            // Send message to queue
            err = rtos_push_to_queue(&os_queue, &my_message, BEKEN_WAIT_FOREVER);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Sent: value=%d\r\n", my_message.value);
            }
            rtos_delay_milliseconds(100);
        }
    }

    void queue_demo_start(void)
    {
        bk_err_t err;
        
        // Create queue with depth of 3
        err = rtos_init_queue(&os_queue, "queue", sizeof(msg_t), 3);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create queue\r\n");
            return;
        }
        
        // Create sender task
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "sender", sender_thread, 0x500, NULL);
        
        // Create receiver task
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "receiver", receiver_thread, 0x500, NULL);
    }

.. note::

    - Queue passes copies of messages, not pointers
    - Sending will block when queue is full, receiving will block when queue is empty
    - Can use rtos_push_to_queue and rtos_push_to_queue_front in interrupts, the function will automatically determine the context
    - Queues can be used for inter-task synchronization and data transfer


**Semaphore:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Create a counting semaphore, set the initial value to 0::

    bk_err_t rtos_init_semaphore( beken_semaphore_t* semaphore,
                                  int maxCount )

- Create a counting semaphore, set the initial value to init_count::

    int rtos_get_semaphore_count( beken_semaphore_t* semaphore )
  
- Get the current semaphore count::

    int rtos_get_semaphore_count( beken_semaphore_t* semaphore )

- Release the semaphore::

    int rtos_set_semaphore( beken_semaphore_t* semaphore )

    Supports in the interrupt context or the task context

- Get the semaphore::

    bk_err_t rtos_get_semaphore( beken_semaphore_t* semaphore, 
                                uint32_t timeout_ms )
    
    Only supports in the task context

    Parameters:
    -Semaphore： the handle of the semaphore
    -timeout_ms: the timeout time, can be configured as
        0: no timeout
        BEKEN_WAIT_FOREVER: block until timeout
        other values: the timeout time

    Return value:
    -kNoErr：semaphore operation successful
    -kGeneralErr or kTimeoutErr: semaphore operation failed

- Delete a created semaphore::

    bk_err_t rtos_deinit_semaphore( beken_semaphore_t* semaphore )
    
    Parameters:
    -semaphore: Semaphore handle pointer
    
    Return value:
    -kNoErr: Delete successful
    -kGeneralErr: Delete failed
    
    **FreeRTOS equivalent interface**: vSemaphoreDelete()

Complete usage example (refer to components/demos/os/os_sem/os_sem.c)::

    #include <os/os.h>

    static beken_semaphore_t os_sem = NULL;

    // Release semaphore task
    void set_semaphore_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        
        while(1) {
            // Release semaphore every 500ms
            rtos_delay_milliseconds(500);
            
            err = rtos_set_semaphore(&os_sem);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Semaphore released\r\n");
            }
        }
    }

    // Acquire semaphore task
    void get_semaphore_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        
        while(1) {
            // Wait for semaphore
            err = rtos_get_semaphore(&os_sem, BEKEN_WAIT_FOREVER);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Semaphore acquired\r\n");
                // Execute synchronized task
            }
        }
    }

    void semaphore_demo_start(void)
    {
        bk_err_t err;
        
        // Create binary semaphore with initial value of 0
        err = rtos_init_semaphore(&os_sem, 1);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create semaphore\r\n");
            return;
        }
        
        // Create acquire semaphore task
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "get_sem", get_semaphore_thread, 0x500, NULL);
        
        // Create release semaphore task
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "set_sem", set_semaphore_thread, 0x500, NULL);
    }

.. note::

    - Counting semaphore is commonly used for resource counting
    - Semaphores have no ownership concept, any task can release or acquire them
    - Acquiring a semaphore decrements the count, releasing increments it
    - Semaphore count will not exceed maxCount
    - Can release semaphore in interrupt, but cannot acquire semaphore in interrupt

**Mutex:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Initialize a mutex::

    bk_err_t rtos_init_mutex( beken_mutex_t* mutex )

- Try to get a mutex, the timeout time is 0::

    bk_err_t rtos_trylock_mutex(beken_mutex_t *mutex)

- Wait indefinitely to get a mutex::

    bk_err_t rtos_lock_mutex( beken_mutex_t* mutex )

- Release a mutex::

    bk_err_t rtos_unlock_mutex( beken_mutex_t* mutex )

- Delete a mutex::

    bk_err_t rtos_deinit_mutex( beken_mutex_t* mutex )

- Initialize a recursive mutex::

    bk_err_t rtos_init_recursive_mutex( beken_mutex_t* mutex )

- Get a recursive mutex::

    bk_err_t rtos_lock_recursive_mutex( beken_mutex_t* mutex )

- Release a recursive mutex::

    bk_err_t rtos_unlock_recursive_mutex( beken_mutex_t* mutex )

- Delete a recursive mutex::

    bk_err_t rtos_deinit_recursive_mutex( beken_mutex_t* mutex )
    
    Parameters:
    -mutex: Recursive mutex handle pointer
    
    Return value:
    -kNoErr: Delete successful
    -kGeneralErr: Delete failed
    
    **FreeRTOS equivalent interface**: vSemaphoreDelete()

Complete usage example (refer to components/demos/os/os_mutex/os_mutex.c)::

    #include <os/os.h>

    static beken_mutex_t os_mutex = NULL;
    static int shared_resource = 0;

    // Function to protect shared resources
    void access_shared_resource(const char *task_name)
    {
        bk_err_t err;
        
        // Lock mutex
        err = rtos_lock_mutex(&os_mutex);
        if (err != kNoErr) {
            BK_LOGI(NULL, "%s: Failed to lock mutex\r\n", task_name);
            return;
        }
        
        // Access shared resource (critical section)
        BK_LOGI(NULL, "%s: Accessing shared resource, value=%d\r\n", 
                 task_name, shared_resource);
        shared_resource++;
        rtos_delay_milliseconds(10);  // Simulate resource access
        
        // Unlock mutex
        err = rtos_unlock_mutex(&os_mutex);
        if (err != kNoErr) {
            BK_LOGI(NULL, "%s: Failed to unlock mutex\r\n", task_name);
        }
    }

    void task1_entry(beken_thread_arg_t arg)
    {
        while(1) {
            access_shared_resource("Task1");
            rtos_delay_milliseconds(100);
        }
    }

    void task2_entry(beken_thread_arg_t arg)
    {
        while(1) {
            access_shared_resource("Task2");
            rtos_delay_milliseconds(100);
        }
    }

    void mutex_demo_start(void)
    {
        bk_err_t err;
        
        // Create mutex
        err = rtos_init_mutex(&os_mutex);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create mutex\r\n");
            return;
        }
        
        // Create two tasks that will compete for shared resource access
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "task1", task1_entry, 0x400, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "task2", task2_entry, 0x400, NULL);
    }

.. note::

    - Mutex has ownership concept, only the task that acquired the mutex can release it
    - Mutex supports priority inheritance to avoid priority inversion problems
    - Cannot use mutex in interrupt
    - Should release mutex as soon as possible after acquiring it, avoid holding for long time
    - Differences between mutex and semaphore:
        * Mutex has ownership, semaphore doesn't
        * Mutex supports priority inheritance, semaphore doesn't
        * Mutex is for resource protection, semaphore is for synchronization and counting
    - Recursive mutex allows the same task to acquire multiple times, needs same number of releases

**Software Timer:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Initialize a single-shot software timer::

    bk_err_t rtos_init_oneshot_timer( beken2_timer_t *timer,
                                      uint32_t time_ms,
                                      timer_2handler_t function,
                                      void* larg,
                                      void* rarg )

- Start a single-shot software timer::

    bk_err_t rtos_start_oneshot_timer( beken2_timer_t* timer )

- Stop a single-shot software timer::

    bk_err_t rtos_stop_oneshot_timer( beken2_timer_t* timer )

- Reset a single-shot software timer::

    bk_err_t rtos_oneshot_reload_timer( beken2_timer_t* timer )

- Reset a single-shot software timer, and reset the timer time and callback::

    bk_err_t rtos_oneshot_reload_timer_ex(beken2_timer_t *timer,
                                          uint32_t time_ms,
                                          timer_2handler_t function,
                                          void *larg,
                                          void *rarg)

- Check if a single-shot software timer is initialized::

    bool rtos_is_oneshot_timer_init( beken2_timer_t* timer )

- Check if a single-shot software timer is running::

    bool rtos_is_oneshot_timer_running( beken2_timer_t* timer )

- Delete a single-shot software timer::

    bk_err_t rtos_deinit_oneshot_timer( beken2_timer_t* timer )

- Create a periodic software timer::

    bk_err_t rtos_init_timer( beken_timer_t *timer,
                              uint32_t time_ms,
                              timer_handler_t function,
                              void* arg )

- Start a periodic software timer::

    bk_err_t rtos_start_timer( beken_timer_t* timer )

- Stop a periodic software timer::

    bk_err_t rtos_stop_timer( beken_timer_t* timer )

- Reset a periodic software timer::

    bk_err_t rtos_reload_timer( beken_timer_t* timer )

- Modify the timer time of a periodic software timer::

    bk_err_t rtos_change_period( beken_timer_t* timer, 
                                 uint32_t time_ms)
                                
- Delete a periodic software timer::  

    bk_err_t rtos_deinit_timer( beken_timer_t* timer )

- Get the expiry time of a periodic software timer::      

    uint32_t rtos_get_timer_expiry_time( beken_timer_t* timer )   

- Check if a periodic software timer is initialized::       

    bool rtos_is_timer_init( beken_timer_t* timer )      

- Check if a periodic software timer is running::        

    bool rtos_is_timer_running( beken_timer_t* timer )
    
    Parameters:
    -timer: Timer handle pointer
    
    Return value:
    -true: Timer is running
    -false: Timer is not running
    
    **FreeRTOS equivalent interface**: xTimerIsTimerActive()

Complete usage example (refer to components/demos/os/os_timer/os_timer.c)::

    #include <os/os.h>

    beken_timer_t timer_handle1, timer_handle2;

    // Periodic timer callback function
    void timer1_callback(void *arg)
    {
        BK_LOGI(NULL, "Timer1 triggered\r\n");
    }

    // One-shot timer callback function
    void timer2_callback(void *larg, void *rarg)
    {
        BK_LOGI(NULL, "Timer2 triggered (one-shot), stopping all timers\r\n");
        
        rtos_stop_timer(&timer_handle1);
        rtos_deinit_timer(&timer_handle1);
    }

    void timer_demo_start(void)
    {
        bk_err_t err;
        
        // Create periodic timer, triggers every 500ms
        err = rtos_init_timer(&timer_handle1, 500, timer1_callback, NULL);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create timer1\r\n");
            return;
        }
        
        // Create one-shot timer, triggers after 2600ms
        err = rtos_init_oneshot_timer(&timer_handle2, 2600, timer2_callback, 
                                      NULL, NULL);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create timer2\r\n");
            return;
        }
        
        // Start periodic timer
        rtos_start_timer(&timer_handle1);
        
        // Start one-shot timer
        rtos_start_oneshot_timer(&timer_handle2);
    }

.. note::

    - The priority of the software timer service task has been set to the highest system priority to ensure timer callbacks can be executed in a timely manner
    - The accuracy of the software timer is related to the system clock tick period. If the system tick number is configured as 1000, the clock tick is 1ms
    - The timer callback function runs in timer service task context, not interrupt context
    - Timer callback functions should be fast in and fast out, and should not use any API interface that may cause the timer task to suspend or block
    - The callback function should not contain a dead loop
    - Not recommended to execute time-consuming operations in timer callback, can use semaphore or queue to notify other tasks
    - One-shot timer only triggers once and automatically stops, periodic timer continues to trigger periodically
    - The actual trigger time of timer may have slight delay, depending on timer service task scheduling
   

.. important::

    **Common Issues with Using Timers After Deletion:**
    
    In practical applications, it is common to encounter issues where software timers are used after being deleted. Common scenarios include:
    
    - **Continuing to use timer after deletion in callback function**: After calling the delete interface in the callback function, if the timer handle is still used for operations (such as start, stop, etc.), it will cause system exceptions
    - **Not checking status after deletion**: After calling the delete interface, the return value or status check interface (such as rtos_is_timer_init()) is not used to confirm successful deletion, and the timer handle is directly used
    - **Race condition in multi-task environment**: One task deletes the timer while another task is still trying to use the timer handle, causing undefined behavior
    - **Handle not set to NULL after deletion**: After deleting the timer, the timer handle pointer is not set to NULL, and subsequent code may incorrectly assume the timer is still valid
    
    **Usage Recommendations:**
    
    - Before deleting a timer, first stop the timer (if it is running), then call the delete interface
    - After deleting a timer, set the timer handle to NULL to avoid subsequent misuse
    - Before using a timer handle, use rtos_is_timer_init() or rtos_is_timer_running() to check the timer status
    - In multi-task environments, use mutexes or other synchronization mechanisms to protect timer creation, usage, and deletion operations
    - Avoid deleting the timer itself in the timer callback function; it is recommended to notify other tasks to perform the deletion through semaphores or queues


**Event:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- Create an event::

    bk_err_t rtos_init_event_flags( beken_event_t* event_flags )

- Wait for event flags::

    beken_event_flags_t rtos_wait_for_event_flags( beken_event_t* event_flags, 
                                    uint32_t flags_to_wait_for, 
                                    beken_bool_t clear_set_flags, 
                                    beken_event_flags_wait_option_t wait_option, 
                                    uint32_t timeout_ms )
    
    Parameters:
    -event_flags: the handle of the event
    -flags_to_wait_for: a bitwise OR value, specifying which positions to wait for in the event group
    -clear_set_flags: set to pdTRUE when xEventGroupWaitBits() waits for the event that satisfies the task wake-up, the system will clear the event flag specified by the parameter uxBitsToWaitFor
    -wait_option：pdTRUE ： when the bits specified by flags_to_wait_for are all set, "logical and" wait for the event, and return the value of the corresponding event flag in the absence of timeout
                             otherwise, this is also called "logical or" wait for the event, and the function returns the value of the corresponding event flag in the absence of timeout
    -timeout_ms：the timeout time

    Return value:
    -Return the event flags that are set in the event, the return value may not be the event flag specified by the user, and needs to be judged and processed again

- Set event flags::

    void rtos_set_event_flags( beken_event_t* event_flags, uint32_t flags_to_set )

    Parameters:
    -event_flags: the handle of the event
    -flags_to_set：the event flag in the event

- Set event flags::

    beken_event_flags_t rtos_clear_event_flags( beken_event_t* event_flags, uint32_t flags_to_clear )

- Synchronize event flags::    

    beken_event_flags_t rtos_sync_event_flags( beken_event_t* event_flags, 
                                               uint32_t flags_to_set, 
                                               uint32_t flags_to_wait_for,
                                               uint32_t timeout_ms)
    
    Atomically set the bits in the event group, and then wait for the combination of bits in the same event group to be set

- Delete an event::

    bk_err_t rtos_deinit_event_flags( beken_event_t* event_flags )

Complete usage example (refer to components/demos/os/os_event/os_event_group.c)::

    #include <os/os.h>
    #include <driver/timer.h>

    #define EVENT_BIT_0  ( 1 << 0 )
    #define EVENT_BIT_3  ( 1 << 3 )
    #define EVENT_BIT_4  ( 1 << 4 )
    #define EVENT_BIT_8  ( 1 << 8 )
    #define EVENT_BIT_15 ( 1 << 15 )
    #define ALL_SYNC_BITS ( EVENT_BIT_4 | EVENT_BIT_8 | EVENT_BIT_15 )

    static beken_event_t event_handler;

    // Timer interrupt callback, set event in interrupt
    static void timer0_isr(timer_id_t timer_id)
    {
        // Set event flag in interrupt context
        rtos_set_event_flags(&event_handler, EVENT_BIT_0);
    }

    // Thread 1: Set EVENT_BIT_3 and participate in synchronization
    static void thread1_function(beken_thread_arg_t arg)
    {
        uint32_t uxReturn;
        
        for(;;) {
            // Set EVENT_BIT_3
            rtos_set_event_flags(&event_handler, EVENT_BIT_3);
            rtos_delay_milliseconds(2000);
            
            // Send EVENT_BIT_4 and wait for all sync bits
            uxReturn = rtos_sync_event_flags(&event_handler, 
                                            EVENT_BIT_4, 
                                            ALL_SYNC_BITS, 
                                            BEKEN_WAIT_FOREVER);
            if ((uxReturn & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
                BK_LOGI(NULL, "Thread1: sync event ok\r\n");
            }
        }
    }

    // Thread 2: Wait for multiple events (logical AND)
    static void thread2_function(beken_thread_arg_t arg)
    {
        uint32_t wait_event;
        
        for (;;) {
            // Wait for both EVENT_BIT_0 and EVENT_BIT_3 (logical AND)
            wait_event = rtos_wait_for_event_flags(&event_handler, 
                                                   EVENT_BIT_0 | EVENT_BIT_3,
                                                   true,  // Auto clear
                                                   WAIT_FOR_ALL_EVENTS,  // Logical AND
                                                   BEKEN_WAIT_FOREVER);
            
            if ((wait_event & (EVENT_BIT_0 | EVENT_BIT_3)) == 
                (EVENT_BIT_0 | EVENT_BIT_3)) {
                BK_LOGI(NULL, "Thread2: got EVENT_BIT_0 & EVENT_BIT_3\r\n");
            }
            
            // Send EVENT_BIT_8 and wait for synchronization
            wait_event = rtos_sync_event_flags(&event_handler, 
                                              EVENT_BIT_8, 
                                              ALL_SYNC_BITS, 
                                              BEKEN_WAIT_FOREVER);
            if ((wait_event & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
                BK_LOGI(NULL, "Thread2: sync event ok\r\n");
            }
        }
    }

    // Thread 3: Wait for multiple events (logical OR)
    static void thread3_function(beken_thread_arg_t arg)
    {
        uint32_t wait_event;
        
        for (;;) {
            // Wait for EVENT_BIT_0 or EVENT_BIT_3 (logical OR)
            wait_event = rtos_wait_for_event_flags(&event_handler, 
                                                   EVENT_BIT_0 | EVENT_BIT_3,
                                                   false,  // Don't auto clear
                                                   WAIT_FOR_ANY_EVENT,  // Logical OR
                                                   BEKEN_WAIT_FOREVER);
            
            // Check which event occurred
            if ((wait_event & (EVENT_BIT_0 | EVENT_BIT_3)) == 
                (EVENT_BIT_0 | EVENT_BIT_3)) {
                BK_LOGI(NULL, "Thread3: got both events\r\n");
            } else if (wait_event & EVENT_BIT_0) {
                BK_LOGI(NULL, "Thread3: got EVENT_BIT_0\r\n");
            } else if (wait_event & EVENT_BIT_3) {
                BK_LOGI(NULL, "Thread3: got EVENT_BIT_3\r\n");
            }
            
            rtos_delay_milliseconds(1000);
            
            // Send EVENT_BIT_15 and wait for synchronization
            wait_event = rtos_sync_event_flags(&event_handler, 
                                              EVENT_BIT_15, 
                                              ALL_SYNC_BITS, 
                                              BEKEN_WAIT_FOREVER);
            if ((wait_event & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
                BK_LOGI(NULL, "Thread3: sync event ok\r\n");
            }
        }
    }

    void os_event_demo_start(void)
    {
        bk_err_t err;
        
        // Initialize timer to trigger every 4 seconds
        bk_timer_driver_init();
        bk_timer_start(TIMER_ID0, 4000, timer0_isr);
        
        // Create event group
        err = rtos_init_event_flags(&event_handler);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create event group\r\n");
            return;
        }
        
        // Create three threads to demonstrate different event waiting modes
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "thread1", thread1_function, 0x1000, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "thread2", thread2_function, 0x1000, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY - 1,
                          "thread3", thread3_function, 0x1000, NULL);
    }

**Example Description:**

This example demonstrates multiple usage scenarios of event groups:

1. **Setting event in interrupt**: Timer interrupt sets EVENT_BIT_0
2. **Logical AND wait**: thread2 waits for both EVENT_BIT_0 and EVENT_BIT_3
3. **Logical OR wait**: thread3 waits for either EVENT_BIT_0 or EVENT_BIT_3
4. **Event synchronization**: Three threads synchronize using rtos_sync_event_flags
5. **Auto clear vs manual clear**: thread2 uses auto clear, thread3 doesn't

.. note::
    
    - Event groups support up to 24 event flags (FreeRTOS limitation)
    - Event flags can be combined and support logical AND/OR waiting
    - Event flags can be set in interrupt context
    - Event groups are suitable for one-to-many or many-to-many task synchronization scenarios
    - rtos_sync_event_flags is commonly used to implement multi-task synchronization points (barriers)

**Time Management:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Time-related interfaces are based on the system clock and provide all time-related services to applications. The base time is a single tick, configured by the macro ``CONFIG_FREERTOS_TICK_RATE_HZ``, which defaults to 1000, meaning 1ms per tick.

- Get system tick count::

    uint64_t bk_get_tick(void)
    
    Gets the tick count since system startup, can be used for precise time measurement and timeout judgment
    
    Return value:
    -Returns total ticks since system startup (64-bit)
    
    **FreeRTOS equivalent interface**: xTaskGetTickCount() / xTaskGetTickCountFromISR()
    
    .. note::
        This function automatically determines the calling context (task or interrupt) and can be safely called in both tasks and interrupts

- Get system uptime in seconds::

    uint32_t bk_get_second(void)
    
    Gets the number of seconds the system has been running since startup, equivalent to bk_get_tick() / bk_get_ticks_per_second()
    
    Return value:
    -Returns system uptime in seconds (32-bit)
    
    Usage scenarios:
    - Record system runtime
    - Simple timestamps
    - Timeout judgment (second-level precision)

- Get system tick frequency::

    uint32_t bk_get_ticks_per_second(void)
    
    Gets the number of ticks per second, i.e., the value of configTICK_RATE_HZ
    
    Return value:
    -Returns ticks per second (default 1000, meaning 1000 ticks per second)
    
    **Corresponding FreeRTOS configuration**: configTICK_RATE_HZ

- Get tick period in milliseconds::

    uint32_t bk_get_ms_per_tick(void)
    
    Gets the number of milliseconds corresponding to each tick, equivalent to 1000 / configTICK_RATE_HZ
    
    Return value:
    -Returns milliseconds per tick (default 1ms)
    
    Usage scenarios:
    - Tick to millisecond conversion
    - Time calculations

Usage examples::

    #include <components/system.h>
    
    void time_measurement_example(void)
    {
        uint64_t start_tick, end_tick;
        uint32_t elapsed_ms;
        
        // Record start time
        start_tick = bk_get_tick();
        
        // Perform some operation
        perform_operation();
        
        // Record end time
        end_tick = bk_get_tick();
        
        // Calculate elapsed time (milliseconds)
        elapsed_ms = (end_tick - start_tick) * bk_get_ms_per_tick();
        BK_LOGI(NULL, "Operation took %d ms\r\n", elapsed_ms);
    }
    
    void system_info_example(void)
    {
        uint32_t uptime_seconds;
        uint32_t tick_rate;
        uint32_t ms_per_tick;
        
        // Get system uptime
        uptime_seconds = bk_get_second();
        BK_LOGI(NULL, "System uptime: %d seconds\r\n", uptime_seconds);
        
        // Get system configuration
        tick_rate = bk_get_ticks_per_second();
        ms_per_tick = bk_get_ms_per_tick();
        BK_LOGI(NULL, "Tick rate: %d Hz, %d ms per tick\r\n", 
                 tick_rate, ms_per_tick);
    }
    
    // Timeout detection example
    bool wait_with_timeout(uint32_t timeout_ms)
    {
        uint64_t start_tick = bk_get_tick();
        uint32_t timeout_ticks = timeout_ms / bk_get_ms_per_tick();
        
        while (!condition_met()) {
            if ((bk_get_tick() - start_tick) > timeout_ticks) {
                BK_LOGI(NULL, "Timeout!\r\n");
                return false;
            }
            rtos_delay_milliseconds(10);
        }
        return true;
    }

.. note::

    - Tick count continuously increases during system operation and does not reset
    - Using 64-bit tick count avoids overflow issues (can run for about 1169 years at 500Hz)
    - Time precision is limited by system tick frequency, default is 1ms
    - Can adjust tick frequency by modifying CONFIG_FREERTOS_TICK_RATE_HZ (e.g., 1000 for 1ms precision)
    - Higher tick frequency provides better timer precision but also increases system overhead

**Interrupt and Critical Section:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Interrupts and critical sections are important concepts in operating systems, used to protect shared resources and implement atomic operations. The abstraction layer provides unified interrupt control and critical section management interfaces.

**Interrupt Control Interfaces:**

- Disable interrupts::

    uint32_t rtos_disable_int(void)
    
    Disables global interrupts and returns the previous interrupt state. This function automatically determines the current context (task or interrupt) and calls the appropriate underlying interface.
    
    Underlying Implementation Mechanism:
    
    Uses PRIMASK register for interrupt control
        * Set PRIMASK register to 1: Disables all interrupts except NMI and hard fault
        * Set PRIMASK register to 0: Enables interrupts
        * Uses CPSID I instruction to set PRIMASK, CPSIE I instruction to clear PRIMASK
    
    Return value:
    -Returns the interrupt state flag before disabling, this value must be saved for subsequent interrupt restoration

    **FreeRTOS equivalent interface**: port_disable_interrupts_flag() / vPortEnterCritical()
    
    Usage scenarios:
    - Protect very short critical code sections (critical section interfaces are recommended)
    - Precise control of interrupt enable/disable
    
.. warning::
    - Cannot call any API that may cause task switching during interrupt disabled period
    - Should restore interrupts as soon as possible to avoid affecting system real-time performance
    - Disabling for too long will cause interrupt loss and watchdog reset

- Enable interrupts::

    void rtos_enable_int(uint32_t int_level)
    
    Restores the previously saved interrupt state
    
    Parameters:
    -int_level: Interrupt state flag returned by rtos_disable_int()
    
    **FreeRTOS equivalent interface**: port_enable_interrupts_flag() / vPortExitCritical()

- Check if interrupts are disabled::

    bool rtos_local_irq_disabled(void)
    
    Checks if interrupts are currently disabled
    
    Return value:
    -true: Interrupts are disabled
    -false: Interrupts are not disabled
    
    **FreeRTOS equivalent interface**: platform_local_irq_disabled()

- Check if in interrupt context::

    bool rtos_is_in_interrupt_context(void)
    
    Determines if the current code is running in an interrupt service routine (ISR)
    
    Return value:
    -true: In interrupt context
    -false: In task context
    
    **FreeRTOS equivalent interface**: platform_is_in_interrupt_context()
    
    Usage scenarios:
    - Write generic functions that can be called in both tasks and interrupts
    - Choose different API call methods based on context

**Critical Section Interfaces:**

- Enter critical section::

    uint32_t rtos_enter_critical(void)
    
    Enters critical section by disabling interrupts. Critical sections can be nested, internally managed by a counter.
    Actually implemented by calling rtos_disable_int().
    
    Return value:
    -Returns the interrupt state flag before entering critical section
 
    **FreeRTOS equivalent interface**: vPortEnterCritical() / taskENTER_CRITICAL()
    
- Exit critical section::

    void rtos_exit_critical(uint32_t int_level)
    
    Exits critical section and restores the previously saved interrupt state. Must be used in pairs with rtos_enter_critical().
    
    Parameters:
    -int_level: Interrupt state flag returned by rtos_enter_critical()
    
    **FreeRTOS equivalent interface**: vPortExitCritical() / taskEXIT_CRITICAL()


Usage examples::

    #include <os/os.h>

    // Example 1: Using interrupt disable to protect critical code
    void update_shared_variable(void)
    {
        uint32_t int_level;
        
        // Disable interrupts
        int_level = rtos_disable_int();
        
        // Critical code section - fast in and out
        shared_counter++;
        shared_flag = true;
        
        // Restore interrupts
        rtos_enable_int(int_level);
    }

    // Example 2: Using critical section protection
    void access_critical_resource(void)
    {
        uint32_t int_level;
        
        // Enter critical section
        int_level = rtos_enter_critical();
        
        // Critical code section
        critical_resource.data = new_value;
        critical_resource.updated = true;
        
        // Exit critical section
        rtos_exit_critical(int_level);
    }

    // Example 3: Context-aware function
    void context_aware_function(void)
    {
        if (rtos_is_in_interrupt_context()) {
            // Processing logic in interrupt
            BK_LOGI(NULL, "Running in ISR\r\n");
        } else {
            // Processing logic in task
            BK_LOGI(NULL, "Running in task\r\n");
        }
    }

    // Example 4: Using macros to simplify critical section operations
    void macro_example(void)
    {
        GLOBAL_INT_DECLARATION();
        
        GLOBAL_INT_DISABLE();
        // Critical code section
        shared_data = new_value;
        GLOBAL_INT_RESTORE();
    }

.. note::

    - **Critical Section vs Mutex**:
        * Critical Section: Implemented by disabling interrupts, protects very short code sections (microsecond level)
        * Mutex: Implemented through task scheduling, protects longer code sections (millisecond level), supports priority inheritance
    - Interrupts are disabled in critical sections, cannot call any API that may cause task switching or blocking
    - Critical section code should be as short as possible, usually only used to protect a few assembly instructions
    - When nesting critical sections, must ensure enter and exit are called in pairs
    - Recommended to use GLOBAL_INT_DECLARATION(), GLOBAL_INT_DISABLE(), GLOBAL_INT_RESTORE() macros
    - Long critical section time will affect system real-time performance and interrupt response time
    - Not recommended to call BK_LOGI and other potentially time-consuming functions in critical sections

**Reference Example Code**
--------------------------------------------------------------------

Complete OS abstraction layer usage examples can be found at:

- API header file：``include/os/os.h``
- Task example: ``components/demos/os/os_thread/os_thread.c``
- Queue example: ``components/demos/os/os_queue/os_queue.c``
- Semaphore example: ``components/demos/os/os_sem/os_sem.c``
- Mutex example: ``components/demos/os/os_mutex/os_mutex.c``
- Timer example: ``components/demos/os/os_timer/os_timer.c``
- Event group example: ``components/demos/os/os_event/os_event_group.c``

These examples demonstrate typical usage of the OS abstraction layer APIs and are recommended for developers to reference.

**Example Code Features:**

- ``os_thread``: Demonstrates task creation, deletion and inter-thread cooperation
- ``os_queue``: Demonstrates queue creation, sending and receiving messages
- ``os_sem``: Demonstrates binary semaphore for task synchronization
- ``os_mutex``: Demonstrates mutex protecting shared resources
- ``os_timer``: Demonstrates single-shot and periodic timer usage
- ``os_event``: Demonstrates various event group waiting modes and synchronization mechanisms, including:
    * Setting event flags in interrupt
    * Logical AND wait (all events must be satisfied)
    * Logical OR wait (any event is satisfied)
    * Multi-task event synchronization point
    * Auto clear and manual clear event flags




