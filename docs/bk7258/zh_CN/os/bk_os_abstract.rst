OS 抽象层
=======================

:link_to_translation:`en:[English]`



**OS 抽象层介绍**
--------------------------------------------------------------------

 - OS抽象层主要为了适配Armino平台不同的操作系统
 - 对于不同的操作系统，OS抽象层对外提供一套统一的接口
 - 目前Armino平台OS抽象层支持的操作系统为FreeRTOS.
 - 目前Armino平台posix接口仅支持FreeRTOS V10操作系统，默认关闭，若使用，需要打开CONFIG_FREERTOS_POSIX配置开关
 - API头文件：``include/os/os.h``
 - 实现源文件：``components/bk_rtos/freertos/v10/rtos_pub.c``
 
.. note::

 - 在使用FreeRTOS的posix功能的时候，在引用posix相关头文件之前，需要先引用FreeRTOS_POSIX.h头文件；
 - 可以在components/bk_rtos/freertos/posix/freertos_impl/include/portable/bk/FreeRTOS_POSIX_portable.h中自定义相关配置，比如某些功能或者数据结构想使用自定义或者编译器自带的，可以在该文件中屏蔽掉posix相关功能。
 - 在移植posix时，如果遇到与编译器自带的头文件有冲突的情况，请优先检查以上两条。

**抽象层与原生FreeRTOS的主要差别**
--------------------------------------------------------------------

为了提供统一的操作系统接口，抽象层在以下几个方面与原生FreeRTOS有所不同：

**优先级反转**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在FreeRTOS原生系统中，数值越大代表优先级越高。而在OS抽象层中，为了统一管理，采用了相反的规则：

 - **抽象层优先级**: 数值越小，优先级越高（0为最高优先级，9为最低优先级）
 - **原生FreeRTOS优先级**: 数值越大，优先级越高
 - **转换公式**: ``原生优先级 = RTOS_HIGHEST_PRIORITY - 抽象层优先级``

例如：当您在抽象层设置优先级为7时，实际对应的FreeRTOS原生优先级会根据系统配置的最大优先级自动转换。

**统一的句柄类型**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

抽象层使用void指针类型的统一句柄，屏蔽了不同操作系统的底层实现差异：

 - ``beken_thread_t``: 任务句柄
 - ``beken_queue_t``: 队列句柄
 - ``beken_semaphore_t``: 信号量句柄
 - ``beken_mutex_t``: 互斥锁句柄
 - ``beken_timer_t``: 定时器句柄

**统一的错误码**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

抽象层定义了统一的错误码，而不是直接使用FreeRTOS的pdPASS/pdFAIL等返回值：

 - ``kNoErr``: 操作成功
 - ``kGeneralErr``: 一般错误
 - ``kTimeoutErr``: 超时错误
 - ``kUnsupportedErr``: 不支持的操作

**超时参数统一**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

抽象层使用毫秒作为超时时间单位，并提供了统一的宏定义：

 - ``BEKEN_WAIT_FOREVER`` (0xFFFFFFFF): 永久等待
 - ``BEKEN_NO_WAIT`` (0): 不等待
 - 其他数值: 超时时间（毫秒）

而FreeRTOS原生接口使用系统时钟节拍（tick）作为单位，抽象层会自动进行转换。

**任务名处理方式**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

抽象层与原生FreeRTOS在任务名处理方式上有所不同：

 - **抽象层**: 采用传指针的方式，直接保存传入的任务名指针，不会拷贝字符串内容
 - **原生FreeRTOS**: 采用拷贝的方式，会在内部创建任务名字符串的副本

.. important::

    - 这意味着在创建任务时，传入的任务名字符串必须保持有效（例如使用字符串常量），不能使用栈上的临时变量。


**抽象层API**
------------------------


**任务创建:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- 指定在sram中创建线程::
  
    bk_err_t rtos_create_sram_thread( beken_thread_t* thread, 
                                    uint8_t priority, 
                                    const char* name,
                                    beken_thread_function_t function, 
                                    uint32_t stack_size, 
                                    beken_thread_arg_t arg )

- 指定在psram中创建线程::

    bk_err_t rtos_create_psram_thread( beken_thread_t* thread, 
                                     uint8_t priority,
                                     const char* name,
                                     beken_thread_function_t function, 
                                     uint32_t stack_size, 
                                     beken_thread_arg_t arg )

- 创建任务::

    bk_err_t rtos_create_thread( beken_thread_t* thread, 
                                uint8_t priority, 
                                const char* name,
                                beken_thread_function_t function, 
                                uint32_t stack_size, 
                                beken_thread_arg_t arg )

    参数说明：
    -thread: 指向创建任务的句柄指针。如果传入NULL，则不保存任务句柄
    -priority: 任务优先级，取值范围0-9
        需要注意的是：对于freertos操作系统内核，任务对应的优先级数字越大，则实际的优先级越大。
        而对于应用层而言则相反，创建任务时配置的优先级数字越大，实际的优先级越小。
        这是因为在SDK中，操作系统适配层做了统一的管理。所以用户视角看到的最大优先级为0，最小优先级为9。
        
        优先级表:
            * 0: 最高优先级（系统保留）
            * 1-5: 库任务和SDK内部任务
            * 6-8: 应用任务（推荐使用范围）
            * 9: 最低优先级（接近空闲任务）
        
        推荐值：
            * BEKEN_APPLICATION_PRIORITY (7): 普通应用任务
            * BEKEN_DEFAULT_WORKER_PRIORITY (6): 工作任务
    -name :创建的任务名称字符串
    -function：任务体入口函数指针，函数原型为 void function(beken_thread_arg_t arg)
    -stack_size：任务堆栈的大小，单位为字节。推荐值：
        * 0x400 (1024字节): 简单任务
        * 0x800 (2048字节): 一般任务
        * 0x1000 (4096字节): 复杂任务
    -arg：传递给任务函数体的参数，可以为NULL
    
    返回值：
    -kNoErr：任务创建成功
    -kGeneralErr: 任务创建失败

    该函数内部会依据宏进行区分，如果同时开启了CONFIG_TASK_STACK_IN_PSRAM 和 CONFIG_PSRAM_AS_SYS_MEMORY，则
    调用的是rtos_create_psram_thread，否则调用的是rtos_create_sram_thread
    
    **FreeRTOS对应接口**: xTaskCreate()

    使用示例::

        #include <os/os.h>

        beken_thread_t task_handle = NULL;

        void my_task_entry(beken_thread_arg_t arg)
        {
            int count = 0;
            while(1) {
                BK_LOGI(NULL, "Task running, count=%d\r\n", count++);
                rtos_delay_milliseconds(1000);  // 延时1秒
            }
        }

        void create_task_example(void)
        {
            bk_err_t ret;
                    
            ret = rtos_create_thread(&task_handle,
                                    BEKEN_APPLICATION_PRIORITY,
                                    "my_task",
                                    my_task_entry,
                                    0x800,  // 2KB栈空间
                                    NULL);
            
            if (ret != kNoErr) {
                BK_LOGI(NULL, "Failed to create task\r\n");
            }
        }

.. note::

    - 任务创建后会立即进入就绪态，等待调度器调度运行
    - 任务函数一般设计为无限循环，如果需要退出应调用rtos_delete_thread()
    - 建议在任务中使用延时函数，避免长时间占用CPU
    - 栈空间不足会导致任务运行异常，需要根据实际使用情况设置合适的栈大小



**任务挂起与恢复:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
- 挂起某个任务::

    void rtos_suspend_thread(beken_thread_t* thread)

    参数说明：
    -thread：任务的句柄，如果传入的参数为NULL，代表是挂起或恢复自身

- 在挂起调度器::

    void rtos_suspend_all_thread(void)

- 恢复某个任务::

    void rtos_resume_thread(beken_thread_t* thread)

- 恢复调度器::

    void rtos_resume_all_thread(void)

**其他任务相关接口:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
除了对底层标准的任务接口进行封装外，适配层还提供了以下和任务相关的接口.

- 任务删除::

    bk_err_t rtos_delete_thread( beken_thread_t* thread )

    参数说明：
    -thread: 要删除的任务句柄指针。如果传入NULL，则删除当前任务自身
    
    返回值：
    -kNoErr：任务删除成功
    -kGeneralErr: 任务删除失败
    
    **FreeRTOS对应接口**: vTaskDelete()
    
    使用示例::
    
        // 删除指定任务
        rtos_delete_thread(&task_handle);
        
        // 任务自删除
        void task_entry(beken_thread_arg_t arg)
        {
            // 执行任务逻辑
            BK_LOGI(NULL, "Task complete\r\n");
            
            // 任务完成后自删除
            rtos_delete_thread(NULL);
        }

- 判断指定的线程是否为当前正在运行的线程::

    bool rtos_is_current_thread(beken_thread_t *thread)

    参数说明：
    -thread：指定的任务句柄指针

    返回值：
    -true：是当前任务
    -false: 不是当前任务
    
    **FreeRTOS对应接口**: xTaskGetCurrentTaskHandle()

- 获取当前正在运行的任务句柄::

    beken_thread_t* rtos_get_current_thread(void)

    返回值：
    -返回当前任务句柄指针
    
    **FreeRTOS对应接口**: xTaskGetCurrentTaskHandle()

- 设置指定任务的优先级::

    bk_err_t rtos_thread_set_priority(
                beken_thread_t *thread, 
                int priority)
    
    参数说明：
    -thread: 要设置优先级的任务句柄指针
    -priority: 新的优先级，取值范围0-9
    
    返回值：
    -kNoErr：设置成功
    -kGeneralErr: 设置失败
    
    **FreeRTOS对应接口**: vTaskPrioritySet()

- 将一个任务挂起指定的时间，单位为秒::

    void rtos_thread_sleep(uint32_t seconds)
    
    参数说明：
    -seconds: 休眠时间，单位为秒
    
    **FreeRTOS对应接口**: vTaskDelay()

- 将一个任务挂起指定的毫秒时间::

    bk_err_t rtos_delay_milliseconds(uint32_t num_ms )
    
    参数说明：
    -num_ms: 延时时间，单位为毫秒
    
    返回值：
    -kNoErr：延时成功
    
    **FreeRTOS对应接口**: vTaskDelay()
    
    使用示例::
    
        void periodic_task(beken_thread_arg_t arg)
        {
            while(1) {
                // 执行周期性任务
                process_data();
                
                // 延时100毫秒
                rtos_delay_milliseconds(100);
            }
        }

.. note::

    - 任务延时会触发系统调度，让出CPU给其他任务运行
    - 延时精度与系统时钟节拍（tick）有关，默认为1ms
    - 实际延时时间可能略大于设置值，这是正常现象

**队列:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
队列是任务间通信的重要机制，用于在任务之间传递消息。

- 初始化一个队列::

    bk_err_t rtos_init_queue( beken_queue_t* queue, 
                            const char* name,
                            uint32_t message_size, 
                            uint32_t number_of_messages )
    参数说明：
    -queue：队列句柄指针
    -name: 队列的名称字符串，可以为NULL
    -message_size: 队列中每条消息的大小（字节数）
    -number_of_messages：队列深度，即队列中最多可以存放的消息数量

    返回值：
    -kNoErr：队列创建成功
    -kGeneralErr: 队列创建失败
    
    **FreeRTOS对应接口**: xQueueCreate()
    
    使用示例::
    
        typedef struct {
            int cmd;
            int value;
        } msg_t;
        
        beken_queue_t my_queue = NULL;
        
        // 创建一个能存放10条消息的队列，每条消息大小为msg_t
        ret = rtos_init_queue(&my_queue, 
                             "my_queue", 
                             sizeof(msg_t), 
                             10);

- 往队列尾部发送消息::    

    bk_err_t rtos_push_to_queue( beken_queue_t* queue, 
                                 void* message,
                                 uint32_t timeout_ms )
    
    内部会判断当前是处于中断上下文还是任务上下文，从而调用不同的内核接口实现对应的功能

    参数说明：
    -queue: 队列句柄指针
    -message: 指向要发送的消息数据的指针
    -timeout_ms：超时时间（毫秒），可配置为：
        * 0: 不等待，如果队列满立即返回
        * BEKEN_WAIT_FOREVER: 一直阻塞等待直到发送成功
        * 其他值: 等待指定的超时时间（毫秒）
    
    返回值：
    -kNoErr：消息发送成功
    -kTimeoutErr: 超时，队列满
    -kGeneralErr: 发送失败
    
    **FreeRTOS对应接口**: xQueueSend() / xQueueSendFromISR()

- 往队列头部发送消息::

    bk_err_t rtos_push_to_queue_front( beken_queue_t* queue,   
                                       void* message, 
                                       uint32_t timeout_ms )

    内部会判断当前是处于中断上下文还是任务上下文，从而调用不同的内核接口实现对应的功能
    
    参数说明：与rtos_push_to_queue相同
    
    返回值：
    -kNoErr：消息发送成功
    -kTimeoutErr: 超时
    -kGeneralErr: 发送失败
    
    **FreeRTOS对应接口**: xQueueSendToFront() / xQueueSendToFrontFromISR()

- 从队列接收消息::

    bk_err_t rtos_pop_from_queue( beken_queue_t* queue, 
                                  void* message, 
                                  uint32_t timeout_ms )
    
    内部调用的是xQueueReceive，仅支持在任务上下文使用
    
    参数说明：
    -queue: 队列句柄指针
    -message: 指向接收消息缓冲区的指针
    -timeout_ms：超时时间（毫秒），可配置为：
        * 0: 不等待，如果队列空立即返回
        * BEKEN_WAIT_FOREVER: 一直阻塞等待直到接收成功
        * 其他值: 等待指定的超时时间（毫秒）
    
    返回值：
    -kNoErr：消息接收成功
    -kTimeoutErr: 超时，队列空
    -kGeneralErr: 接收失败
    
    **FreeRTOS对应接口**: xQueueReceive()

- 删除一个队列::

    bk_err_t rtos_deinit_queue( beken_queue_t* queue )
    
    参数说明：
    -queue: 队列句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: vQueueDelete()

- 判断一个队列是否为空::

    bool rtos_is_queue_empty( beken_queue_t* queue )

    仅能在中断上下文中调用
    
    参数说明：
    -queue: 队列句柄指针
    
    返回值：
    -true: 队列为空
    -false: 队列不为空
    
    **FreeRTOS对应接口**: xQueueIsQueueEmptyFromISR()

- 判断一个队列是否已满::

    bool rtos_is_queue_full( beken_queue_t* queue )

    仅能在中断上下文中调用
    
    参数说明：
    -queue: 队列句柄指针
    
    返回值：
    -true: 队列已满
    -false: 队列未满
    
    **FreeRTOS对应接口**: xQueueIsQueueFullFromISR()

完整使用示例（参考components/demos/os/os_queue/os_queue.c）::

    #include <os/os.h>

    typedef struct {
        int value;
    } msg_t;

    static beken_queue_t os_queue = NULL;

    // 接收任务
    void receiver_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        msg_t received = {0};
        
        while(1) {
            // 从队列接收消息，永久等待
            err = rtos_pop_from_queue(&os_queue, &received, BEKEN_WAIT_FOREVER);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Received: value=%d\r\n", received.value);
            }
        }
    }

    // 发送任务
    void sender_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        msg_t my_message = {0};
        
        while(1) {
            my_message.value++;
            // 向队列发送消息
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
        
        // 创建队列，深度为3
        err = rtos_init_queue(&os_queue, "queue", sizeof(msg_t), 3);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create queue\r\n");
            return;
        }
        
        // 创建发送任务
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "sender", sender_thread, 0x500, NULL);
        
        // 创建接收任务
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "receiver", receiver_thread, 0x500, NULL);
    }

.. note::

    - 队列传递的是消息的拷贝，不是指针
    - 队列满时发送会阻塞，队列空时接收会阻塞
    - 可以在中断中使用rtos_push_to_queue和rtos_push_to_queue_front，函数内部会自动判断上下文
    - 队列可用于任务间同步和数据传递


**信号量:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
信号量用于任务间同步和资源计数。

- 创建一个计数信号量，设置初始值为0::

    bk_err_t rtos_init_semaphore( beken_semaphore_t* semaphore,
                                  int maxCount )
    
    参数说明：
    -semaphore: 信号量句柄指针
    -maxCount: 最大计数值，通常设置为1或更大值
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    .. note::
        此函数创建的信号量初始值为0，如需设置初始值请使用rtos_init_semaphore_ex
    
    **FreeRTOS对应接口**: xSemaphoreCreateCounting()

- 创建一个计数信号量，可设置初始值::

    bk_err_t rtos_init_semaphore_ex( beken_semaphore_t* semaphore,
                                     const char* name,
                                     int maxCount,
                                     int init_count )
    
    参数说明：
    -semaphore: 信号量句柄指针
    -name: 信号量名称，可以为NULL
    -maxCount: 最大计数值
    -init_count: 初始计数值
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xSemaphoreCreateCounting()
  
- 获取当前的信号量计数值::

    int rtos_get_semaphore_count( beken_semaphore_t* semaphore )
    
    参数说明：
    -semaphore: 信号量句柄指针
    
    返回值：
    -返回当前信号量的计数值
    
    **FreeRTOS对应接口**: uxSemaphoreGetCount()

- 释放信号量::

    int rtos_set_semaphore( beken_semaphore_t* semaphore )

    支持在中断上下文或任务上下文使用，函数内部会自动判断
    
    参数说明：
    -semaphore: 信号量句柄指针
    
    返回值：
    -kNoErr：释放成功
    -kGeneralErr: 释放失败
    
    **FreeRTOS对应接口**: xSemaphoreGive() / xSemaphoreGiveFromISR()

- 获取信号量::

    bk_err_t rtos_get_semaphore( beken_semaphore_t* semaphore, 
                                uint32_t timeout_ms )
    
    仅支持在任务上下文使用

    参数说明：
    -semaphore： 信号量句柄指针
    -timeout_ms: 超时时间（毫秒），可配置为：
        * 0: 不等待，立即返回
        * BEKEN_WAIT_FOREVER: 一直阻塞等待直到获取成功
        * 其他值: 等待指定的超时时间（毫秒）

    返回值：
    -kNoErr：获取成功
    -kTimeoutErr: 超时
    -kGeneralErr: 获取失败
    
    **FreeRTOS对应接口**: xSemaphoreTake()

- 删除一个已创建的信号量::

    bk_err_t rtos_deinit_semaphore( beken_semaphore_t* semaphore )
    
    参数说明：
    -semaphore: 信号量句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: vSemaphoreDelete()

完整使用示例（参考components/demos/os/os_sem/os_sem.c）::

    #include <os/os.h>

    static beken_semaphore_t os_sem = NULL;

    // 释放信号量的任务
    void set_semaphore_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        
        while(1) {
            // 每500ms释放一次信号量
            rtos_delay_milliseconds(500);
            
            err = rtos_set_semaphore(&os_sem);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Semaphore released\r\n");
            }
        }
    }

    // 获取信号量的任务
    void get_semaphore_thread(beken_thread_arg_t arg)
    {
        bk_err_t err;
        
        while(1) {
            // 等待信号量
            err = rtos_get_semaphore(&os_sem, BEKEN_WAIT_FOREVER);
            if (err == kNoErr) {
                BK_LOGI(NULL, "Semaphore acquired\r\n");
                // 执行同步任务
            }
        }
    }

    void semaphore_demo_start(void)
    {
        bk_err_t err;
        
        // 创建二值信号量，初始值为0
        err = rtos_init_semaphore(&os_sem, 1);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create semaphore\r\n");
            return;
        }
        
        // 创建获取信号量任务
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "get_sem", get_semaphore_thread, 0x500, NULL);
        
        // 创建释放信号量任务
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "set_sem", set_semaphore_thread, 0x500, NULL);
    }

.. note::

    - 计数信号量常用于资源计数
    - 信号量不具有所有权概念，任何任务都可以释放或获取
    - 获取信号量会将计数值减1，释放信号量会将计数值加1
    - 信号量计数值不会超过maxCount
    - 可以在中断中释放信号量，但不能在中断中获取信号量

**互斥锁:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
互斥锁用于保护共享资源，防止多个任务同时访问。互斥锁具有优先级继承机制，可以防止优先级反转问题。

- 初始化一个互斥锁::

    bk_err_t rtos_init_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 互斥锁句柄指针
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xSemaphoreCreateMutex()

- 尝试获取一个互斥锁，超时时间为0::

    bk_err_t rtos_trylock_mutex(beken_mutex_t *mutex)
    
    参数说明：
    -mutex: 互斥锁句柄指针
    
    返回值：
    -kNoErr：获取成功
    -kGeneralErr: 获取失败（互斥锁已被占用）
    
    **FreeRTOS对应接口**: xSemaphoreTake(mutex, 0)

- 无限等待获取一个互斥锁::

    bk_err_t rtos_lock_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 互斥锁句柄指针
    
    返回值：
    -kNoErr：获取成功
    -kGeneralErr: 获取失败
    
    **FreeRTOS对应接口**: xSemaphoreTake(mutex, portMAX_DELAY)

- 释放一个互斥锁::

    bk_err_t rtos_unlock_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 互斥锁句柄指针
    
    返回值：
    -kNoErr：释放成功
    -kGeneralErr: 释放失败
    
    **FreeRTOS对应接口**: xSemaphoreGive()

- 删除一个互斥锁::

    bk_err_t rtos_deinit_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 互斥锁句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: vSemaphoreDelete()

- 初始化一个递归互斥锁::

    bk_err_t rtos_init_recursive_mutex( beken_mutex_t* mutex )
    
    递归互斥锁允许同一任务多次获取同一互斥锁，需要对应次数的释放操作
    
    参数说明：
    -mutex: 递归互斥锁句柄指针
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xSemaphoreCreateRecursiveMutex()

- 获取一个递归互斥锁::

    bk_err_t rtos_lock_recursive_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 递归互斥锁句柄指针
    
    返回值：
    -kNoErr：获取成功
    -kGeneralErr: 获取失败
    
    **FreeRTOS对应接口**: xSemaphoreTakeRecursive()

- 释放一个递归互斥锁::

    bk_err_t rtos_unlock_recursive_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 递归互斥锁句柄指针
    
    返回值：
    -kNoErr：释放成功
    -kGeneralErr: 释放失败
    
    **FreeRTOS对应接口**: xSemaphoreGiveRecursive()

- 删除一个递归互斥锁::

    bk_err_t rtos_deinit_recursive_mutex( beken_mutex_t* mutex )
    
    参数说明：
    -mutex: 递归互斥锁句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: vSemaphoreDelete()

完整使用示例（参考components/demos/os/os_mutex/os_mutex.c）::

    #include <os/os.h>

    static beken_mutex_t os_mutex = NULL;
    static int shared_resource = 0;

    // 保护共享资源的函数
    void access_shared_resource(const char *task_name)
    {
        bk_err_t err;
        
        // 获取互斥锁
        err = rtos_lock_mutex(&os_mutex);
        if (err != kNoErr) {
            BK_LOGI(NULL, "%s: Failed to lock mutex\r\n", task_name);
            return;
        }
        
        // 访问共享资源（临界区）
        BK_LOGI(NULL, "%s: Accessing shared resource, value=%d\r\n", 
                 task_name, shared_resource);
        shared_resource++;
        rtos_delay_milliseconds(10);  // 模拟资源访问
        
        // 释放互斥锁
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
        
        // 创建互斥锁
        err = rtos_init_mutex(&os_mutex);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create mutex\r\n");
            return;
        }
        
        // 创建两个任务，它们将竞争访问共享资源
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "task1", task1_entry, 0x400, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "task2", task2_entry, 0x400, NULL);
    }

.. note::

    - 互斥锁具有所有权概念，只有获取互斥锁的任务才能释放它
    - 互斥锁支持优先级继承，可以避免优先级反转问题
    - 不能在中断中使用互斥锁
    - 获取互斥锁后应尽快释放，避免长时间持有
    - 互斥锁和信号量的区别：
        * 互斥锁有所有权，信号量没有
        * 互斥锁支持优先级继承，信号量不支持
        * 互斥锁用于资源保护，信号量用于同步和计数
    - 递归互斥锁允许同一任务多次获取，需要相同次数的释放操作

**软件定时器:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
软件定时器运行在定时器服务任务中，可以在指定时间后执行回调函数。分为单次定时器和周期定时器。

**单次定时器相关接口:**

- 初始化一个单次软件定时器::

    bk_err_t rtos_init_oneshot_timer( beken2_timer_t *timer,
                                      uint32_t time_ms,
                                      timer_2handler_t function,
                                      void* larg,
                                      void* rarg )
    
    参数说明：
    -timer: 定时器句柄指针
    -time_ms: 定时时间（毫秒）
    -function: 定时器回调函数，原型为 void function(void *larg, void *rarg)
    -larg: 回调函数的左参数
    -rarg: 回调函数的右参数
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xTimerCreate()

- 开启单次软件定时器::

    bk_err_t rtos_start_oneshot_timer( beken2_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：启动成功
    -kGeneralErr: 启动失败
    
    **FreeRTOS对应接口**: xTimerStart()

- 停止单次软件定时器::

    bk_err_t rtos_stop_oneshot_timer( beken2_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：停止成功
    -kGeneralErr: 停止失败
    
    **FreeRTOS对应接口**: xTimerStop()

- 复位单次软件定时器::

    bk_err_t rtos_oneshot_reload_timer( beken2_timer_t* timer )
    
    重新启动定时器，从当前时间重新计时
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：复位成功
    -kGeneralErr: 复位失败
    
    **FreeRTOS对应接口**: xTimerReset()

- 复位单次软件定时器，并重新设置定时时间与回调::

    bk_err_t rtos_oneshot_reload_timer_ex(beken2_timer_t *timer,
                                          uint32_t time_ms,
                                          timer_2handler_t function,
                                          void *larg,
                                          void *rarg)
    
    参数说明：与rtos_init_oneshot_timer相同
    
    返回值：
    -kNoErr：复位成功
    -kGeneralErr: 复位失败

- 判断单次软件定时器是否初始化::

    bool rtos_is_oneshot_timer_init( beken2_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -true: 已初始化
    -false: 未初始化

- 判断单次软件定时器是否在运行::

    bool rtos_is_oneshot_timer_running( beken2_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -true: 正在运行
    -false: 未运行

- 删除单次软件定时器::

    bk_err_t rtos_deinit_oneshot_timer( beken2_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: xTimerDelete()

**周期定时器相关接口:**

- 创建周期软件定时器::

    bk_err_t rtos_init_timer( beken_timer_t *timer,
                              uint32_t time_ms,
                              timer_handler_t function,
                              void* arg )
    
    参数说明：
    -timer: 定时器句柄指针
    -time_ms: 定时周期（毫秒）
    -function: 定时器回调函数，原型为 void function(void *arg)
    -arg: 回调函数的参数
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xTimerCreate()

- 开启周期软件定时器::

    bk_err_t rtos_start_timer( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：启动成功
    -kGeneralErr: 启动失败
    
    **FreeRTOS对应接口**: xTimerStart()

- 停止周期软件定时器::

    bk_err_t rtos_stop_timer( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：停止成功
    -kGeneralErr: 停止失败
    
    **FreeRTOS对应接口**: xTimerStop()

- 复位周期软件定时器::

    bk_err_t rtos_reload_timer( beken_timer_t* timer )
    
    重新启动定时器，从当前时间重新计时
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：复位成功
    -kGeneralErr: 复位失败
    
    **FreeRTOS对应接口**: xTimerReset()

- 修改周期软件定时器定时时间::

    bk_err_t rtos_change_period( beken_timer_t* timer, 
                                 uint32_t time_ms)
    
    参数说明：
    -timer: 定时器句柄指针
    -time_ms: 新的定时周期（毫秒）
    
    返回值：
    -kNoErr：修改成功
    -kGeneralErr: 修改失败
    
    **FreeRTOS对应接口**: xTimerChangePeriod()
                                
- 删除周期软件定时器::  

    bk_err_t rtos_deinit_timer( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: xTimerDelete()

- 获取周期软件定时器到期时间::      

    uint32_t rtos_get_timer_expiry_time( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -返回定时器到期的系统时间（tick）
    
    **FreeRTOS对应接口**: xTimerGetExpiryTime()

- 判断周期软件定时器是否初始化::       

    bool rtos_is_timer_init( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -true: 已初始化
    -false: 未初始化

- 判断周期软件定时器是否在运行::        

    bool rtos_is_timer_running( beken_timer_t* timer )
    
    参数说明：
    -timer: 定时器句柄指针
    
    返回值：
    -true: 正在运行
    -false: 未运行
    
    **FreeRTOS对应接口**: xTimerIsTimerActive()

完整使用示例（参考components/demos/os/os_timer/os_timer.c）::

    #include <os/os.h>

    beken_timer_t timer_handle1, timer_handle2;

    // 周期定时器回调函数
    void timer1_callback(void *arg)
    {
        BK_LOGI(NULL, "Timer1 triggered\r\n");
    }

    // 单次定时器回调函数
    void timer2_callback(void *larg, void *rarg)
    {
        BK_LOGI(NULL, "Timer2 triggered (one-shot), stopping all timers\r\n");
        
        rtos_stop_timer(&timer_handle1);
        rtos_deinit_timer(&timer_handle1);
    }

    void timer_demo_start(void)
    {
        bk_err_t err;
        
        // 创建周期定时器，每500ms触发一次
        err = rtos_init_timer(&timer_handle1, 500, timer1_callback, NULL);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create timer1\r\n");
            return;
        }
        
        // 创建单次定时器，2600ms后触发一次
        err = rtos_init_oneshot_timer(&timer_handle2, 2600, timer2_callback, 
                                      NULL, NULL);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create timer2\r\n");
            return;
        }
        
        // 启动周期定时器
        rtos_start_timer(&timer_handle1);
        
        // 启动单次定时器
        rtos_start_oneshot_timer(&timer_handle2);
    }

.. note::

    - 当前软件定时器服务任务的优先级已被设置为系统最高优先级，以确保定时器回调能够及时执行
    - 软件定时器的精度与系统时钟节拍周期有关，如果配置的系统节拍数为1000，也就是节拍时钟为1ms
    - 软件定时器的回调函数运行在定时器服务任务上下文中，不是中断上下文
    - 定时器回调函数中应快进快出，不允许使用任何可能引起软件定时器任务挂起或者阻塞的API接口
    - 在回调函数中不能出现死循环
    - 不建议在定时器回调中执行耗时操作，可以使用信号量或队列通知其他任务处理
    - 单次定时器只触发一次后自动停止，周期定时器会持续周期性触发
    - 定时器的实际触发时间可能会有轻微延迟，这取决于定时器服务任务的调度情况
    

.. important::

    **定时器删除后继续使用的常见问题：**
    
    实际应用中经常遇到软件定时器已经被删除后又继续使用的问题，常见场景包括：
    
    - **在定时器回调函数中删除定时器后继续使用**：在回调函数中调用删除接口后，如果继续使用该定时器句柄进行操作（如启动、停止等），会导致系统异常
    - **删除定时器后未检查状态**：调用删除接口后没有检查返回值或使用状态检查接口（如rtos_is_timer_init()）确认删除成功，直接继续使用定时器句柄
    - **多任务环境下竞争使用**：一个任务删除定时器，另一个任务仍在尝试使用该定时器句柄，导致未定义行为
    - **删除后句柄未置NULL**：删除定时器后，定时器句柄指针没有置为NULL，后续代码可能误判定时器仍有效
    
    **使用建议：**
    
    - 删除定时器前，先停止定时器（如果正在运行），然后调用删除接口
    - 删除定时器后，将定时器句柄置为NULL，避免后续误用
    - 在使用定时器句柄前，使用rtos_is_timer_init()或rtos_is_timer_running()检查定时器状态
    - 在多任务环境下，使用互斥锁或其他同步机制保护定时器的创建、使用和删除操作
    - 避免在定时器回调函数中删除自身定时器，建议通过信号量或队列通知其他任务执行删除操作


**事件:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
事件组用于任务间的事件同步，一个事件组包含多个事件标志位，任务可以等待一个或多个事件标志位。

- 创建事件::

    bk_err_t rtos_init_event_flags( beken_event_t* event_flags )
    
    参数说明：
    -event_flags: 事件句柄指针
    
    返回值：
    -kNoErr：创建成功
    -kGeneralErr: 创建失败
    
    **FreeRTOS对应接口**: xEventGroupCreate()

- 等待事件标志::

    beken_event_flags_t rtos_wait_for_event_flags( beken_event_t* event_flags, 
                                    uint32_t flags_to_wait_for, 
                                    beken_bool_t clear_set_flags, 
                                    beken_event_flags_wait_option_t wait_option, 
                                    uint32_t timeout_ms )
    
    参数说明：
    -event_flags: 事件句柄指针
    -flags_to_wait_for: 一个按位或的值，指定需要等待事件组中的哪些位置1
        例如：0x01 | 0x04 表示等待第0位和第2位
    -clear_set_flags: 是否自动清除事件标志位
        * 1 (pdTRUE): 等待成功后自动清除指定的事件标志位
        * 0 (pdFALSE): 不清除事件标志位
    -wait_option: 等待选项
        * WAIT_FOR_ALL_EVENTS (pdTRUE): 逻辑与，flags_to_wait_for指定的所有位都置位时才返回
        * WAIT_FOR_ANY_EVENT (pdFALSE): 逻辑或，flags_to_wait_for指定的任意位置位时就返回
    -timeout_ms：超时等待时间（毫秒）
        * 0: 不等待，立即返回
        * BEKEN_WAIT_FOREVER: 永久等待
        * 其他值: 等待指定时间

    返回值：
    -返回事件组中当前置位的标志位值，需要判断是否是期望的事件位
    -如果超时，返回值可能不包含期望的事件位
    
    **FreeRTOS对应接口**: xEventGroupWaitBits()

- 设置事件标志::

    void rtos_set_event_flags( beken_event_t* event_flags, uint32_t flags_to_set )

    参数说明：
    -event_flags: 事件句柄指针
    -flags_to_set：指定要置位的事件标志位，例如 0x01 | 0x04 表示设置第0位和第2位
    
    **FreeRTOS对应接口**: xEventGroupSetBits()

- 清除事件标志::

    beken_event_flags_t rtos_clear_event_flags( beken_event_t* event_flags, uint32_t flags_to_clear )
    
    参数说明：
    -event_flags: 事件句柄指针
    -flags_to_clear：指定要清除的事件标志位
    
    返回值：
    -返回清除前的事件标志位值
    
    **FreeRTOS对应接口**: xEventGroupClearBits()

- 同步事件标志::

    beken_event_flags_t rtos_sync_event_flags( beken_event_t* event_flags, 
                                               uint32_t flags_to_set, 
                                               uint32_t flags_to_wait_for,
                                               uint32_t timeout_ms)
    
    原子地设置事件组中的位，然后等待同一事件组中的位组合被设置。常用于多任务同步点。
    
    参数说明：
    -event_flags: 事件句柄指针
    -flags_to_set：要设置的事件标志位
    -flags_to_wait_for：要等待的事件标志位（通常是所有任务需要同步的位）
    -timeout_ms：超时时间（毫秒）
    
    返回值：
    -返回事件组中当前置位的标志位值
    
    **FreeRTOS对应接口**: xEventGroupSync()

- 删除事件::

    bk_err_t rtos_deinit_event_flags( beken_event_t* event_flags )
    
    参数说明：
    -event_flags: 事件句柄指针
    
    返回值：
    -kNoErr：删除成功
    -kGeneralErr: 删除失败
    
    **FreeRTOS对应接口**: vEventGroupDelete()

完整使用示例（参考components/demos/os/os_event/os_event_group.c）::

    #include <os/os.h>
    #include <driver/timer.h>

    #define EVENT_BIT_0  ( 1 << 0 )
    #define EVENT_BIT_3  ( 1 << 3 )
    #define EVENT_BIT_4  ( 1 << 4 )
    #define EVENT_BIT_8  ( 1 << 8 )
    #define EVENT_BIT_15 ( 1 << 15 )
    #define ALL_SYNC_BITS ( EVENT_BIT_4 | EVENT_BIT_8 | EVENT_BIT_15 )

    static beken_event_t event_handler;

    // 定时器中断回调，在中断中设置事件
    static void timer0_isr(timer_id_t timer_id)
    {
        // 在中断上下文中设置事件标志
        rtos_set_event_flags(&event_handler, EVENT_BIT_0);
    }

    // 线程1：设置EVENT_BIT_3，并参与同步
    static void thread1_function(beken_thread_arg_t arg)
    {
        uint32_t uxReturn;
        
        for(;;) {
            // 设置EVENT_BIT_3
            rtos_set_event_flags(&event_handler, EVENT_BIT_3);
            rtos_delay_milliseconds(2000);
            
            // 发送EVENT_BIT_4并等待所有同步位
            uxReturn = rtos_sync_event_flags(&event_handler, 
                                            EVENT_BIT_4, 
                                            ALL_SYNC_BITS, 
                                            BEKEN_WAIT_FOREVER);
            if ((uxReturn & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
                BK_LOGI(NULL, "Thread1: sync event ok\r\n");
            }
        }
    }

    // 线程2：等待多个事件（逻辑与），并参与同步
    static void thread2_function(beken_thread_arg_t arg)
    {
        uint32_t wait_event;
        
        for (;;) {
            // 等待EVENT_BIT_0和EVENT_BIT_3都置位（逻辑与）
            wait_event = rtos_wait_for_event_flags(&event_handler, 
                                                   EVENT_BIT_0 | EVENT_BIT_3,
                                                   true,  // 自动清除
                                                   WAIT_FOR_ALL_EVENTS,  // 逻辑与
                                                   BEKEN_WAIT_FOREVER);
            
            if ((wait_event & (EVENT_BIT_0 | EVENT_BIT_3)) == 
                (EVENT_BIT_0 | EVENT_BIT_3)) {
                BK_LOGI(NULL, "Thread2: got EVENT_BIT_0 & EVENT_BIT_3\r\n");
            }
            
            // 发送EVENT_BIT_8并等待同步
            wait_event = rtos_sync_event_flags(&event_handler, 
                                              EVENT_BIT_8, 
                                              ALL_SYNC_BITS, 
                                              BEKEN_WAIT_FOREVER);
            if ((wait_event & ALL_SYNC_BITS) == ALL_SYNC_BITS) {
                BK_LOGI(NULL, "Thread2: sync event ok\r\n");
            }
        }
    }

    // 线程3：等待多个事件（逻辑或），并参与同步
    static void thread3_function(beken_thread_arg_t arg)
    {
        uint32_t wait_event;
        
        for (;;) {
            // 等待EVENT_BIT_0或EVENT_BIT_3任一置位（逻辑或）
            wait_event = rtos_wait_for_event_flags(&event_handler, 
                                                   EVENT_BIT_0 | EVENT_BIT_3,
                                                   false,  // 不自动清除
                                                   WAIT_FOR_ANY_EVENT,  // 逻辑或
                                                   BEKEN_WAIT_FOREVER);
            
            // 检查具体是哪个事件
            if ((wait_event & (EVENT_BIT_0 | EVENT_BIT_3)) == 
                (EVENT_BIT_0 | EVENT_BIT_3)) {
                BK_LOGI(NULL, "Thread3: got both events\r\n");
            } else if (wait_event & EVENT_BIT_0) {
                BK_LOGI(NULL, "Thread3: got EVENT_BIT_0\r\n");
            } else if (wait_event & EVENT_BIT_3) {
                BK_LOGI(NULL, "Thread3: got EVENT_BIT_3\r\n");
            }
            
            rtos_delay_milliseconds(1000);
            
            // 发送EVENT_BIT_15并等待同步
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
        
        // 初始化定时器，每4秒触发一次中断
        bk_timer_driver_init();
        bk_timer_start(TIMER_ID0, 4000, timer0_isr);
        
        // 创建事件组
        err = rtos_init_event_flags(&event_handler);
        if (err != kNoErr) {
            BK_LOGI(NULL, "Failed to create event group\r\n");
            return;
        }
        
        // 创建三个线程，演示不同的事件等待方式
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "thread1", thread1_function, 0x1000, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY,
                          "thread2", thread2_function, 0x1000, NULL);
        
        rtos_create_thread(NULL, BEKEN_APPLICATION_PRIORITY - 1,
                          "thread3", thread3_function, 0x1000, NULL);
    }

**示例说明：**

此示例展示了事件组的多种使用场景：

1. **中断中设置事件**：定时器中断中设置EVENT_BIT_0
2. **逻辑与等待**：thread2等待EVENT_BIT_0和EVENT_BIT_3同时置位
3. **逻辑或等待**：thread3等待EVENT_BIT_0或EVENT_BIT_3任一置位
4. **事件同步**：三个线程使用rtos_sync_event_flags实现同步点
5. **自动清除与手动清除**：thread2使用自动清除，thread3不自动清除

.. note::
    
    - 事件组最多支持24位事件标志（FreeRTOS限制）
    - 事件标志位可以组合使用，支持逻辑与/或等待
    - 可以在中断中设置事件标志
    - 事件组适合一对多或多对多的任务同步场景
    - rtos_sync_event_flags常用于实现多任务同步点（屏障）

**时间管理:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
时间相关的接口以系统时钟为基础，提供给应用程序所有和时间有关的服务。基准时间为单个tick的时间，由宏``CONFIG_FREERTOS_TICK_RATE_HZ``配置，该值默认为1000，即1ms一个tick。

- 获取系统tick计数::

    uint64_t bk_get_tick(void)
    
    获取自系统启动以来的tick计数值，可用于精确的时间测量和超时判断
    
    返回值：
    -返回系统启动以来的tick总数（64位）
    
    **FreeRTOS对应接口**: xTaskGetTickCount() / xTaskGetTickCountFromISR()
    
    .. note::
        此函数会自动判断调用上下文（任务或中断），可以在任务和中断中安全调用

- 获取系统运行秒数::

    uint32_t bk_get_second(void)
    
    获取系统启动以来运行的秒数，相当于 bk_get_tick() / bk_get_ticks_per_second()
    
    返回值：
    -返回系统运行的秒数（32位）
    
    使用场景：
    - 记录系统运行时间
    - 简单的时间戳
    - 超时判断（秒级精度）

- 获取系统tick频率::

    uint32_t bk_get_ticks_per_second(void)
    
    获取系统每秒的tick数，即configTICK_RATE_HZ的值
    
    返回值：
    -返回每秒tick数（默认1000，即每秒1000个tick）
    
    **对应FreeRTOS配置**: configTICK_RATE_HZ

- 获取tick周期（毫秒）::

    uint32_t bk_get_ms_per_tick(void)
    
    获取每个tick对应的毫秒数，相当于 1000 / configTICK_RATE_HZ
    
    返回值：
    -返回每个tick的毫秒数（默认1ms）
    
    使用场景：
    - tick到毫秒的转换
    - 时间计算

使用示例::

    #include <components/system.h>
    
    void time_measurement_example(void)
    {
        uint64_t start_tick, end_tick;
        uint32_t elapsed_ms;
        
        // 记录开始时间
        start_tick = bk_get_tick();
        
        // 执行某些操作
        perform_operation();
        
        // 记录结束时间
        end_tick = bk_get_tick();
        
        // 计算耗时（毫秒）
        elapsed_ms = (end_tick - start_tick) * bk_get_ms_per_tick();
        BK_LOGI(NULL, "Operation took %d ms\r\n", elapsed_ms);
    }
    
    void system_info_example(void)
    {
        uint32_t uptime_seconds;
        uint32_t tick_rate;
        uint32_t ms_per_tick;
        
        // 获取系统运行时间
        uptime_seconds = bk_get_second();
        BK_LOGI(NULL, "System uptime: %d seconds\r\n", uptime_seconds);
        
        // 获取系统配置
        tick_rate = bk_get_ticks_per_second();
        ms_per_tick = bk_get_ms_per_tick();
        BK_LOGI(NULL, "Tick rate: %d Hz, %d ms per tick\r\n", 
                 tick_rate, ms_per_tick);
    }
    
    // 超时检测示例
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

    - tick计数会在系统运行过程中持续增加，不会复位
    - 使用64位tick计数可以避免溢出问题（在500Hz时可运行约1169年）
    - 时间精度受系统tick频率限制，默认为1ms
    - 可以通过修改CONFIG_FREERTOS_TICK_RATE_HZ来调整tick频率（例如1000表示1ms精度）
    - tick频率越高，定时器精度越高，但系统开销也越大

**中断与临界区:**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
中断与临界区是操作系统中的重要概念，用于保护共享资源和实现原子操作。抽象层提供了统一的中断控制和临界区管理接口。

**中断控制接口:**

- 禁用中断::

    uint32_t rtos_disable_int(void)
    
    禁用全局中断，返回之前的中断状态。此函数会自动判断当前上下文（任务或中断）并调用相应的底层接口。
    
    底层实现机制：
    
        使用PRIMASK寄存器实现中断控制
        * 设置PRIMASK寄存器值为1：禁止除NMI和硬fault外的所有中断
        * 设置PRIMASK寄存器值为0：使能中断
        * 通过CPSID I指令设置PRIMASK，通过CPSIE I指令清除PRIMASK
    
    
    返回值：
    -返回禁用前的中断状态标志，需要保存此值用于后续恢复中断

    **FreeRTOS对应接口**: port_disable_interrupts_flag() / vPortEnterCritical()
    
    使用场景：
    - 保护非常短的临界代码段（建议使用临界区接口）
    - 需要精确控制中断使能/禁用的场合
    
.. warning::
    - 禁用中断期间不能调用任何可能引起任务切换的API
    - 应尽快恢复中断，避免影响系统实时性
    - 禁用时间过长会导致中断丢失和看门狗复位

- 启用中断::

    void rtos_enable_int(uint32_t int_level)
    
    恢复之前保存的中断状态
    
    参数说明：
    -int_level: 由rtos_disable_int()返回的中断状态标志
    
    **FreeRTOS对应接口**: port_enable_interrupts_flag() / vPortExitCritical()

- 检查中断是否禁用::

    bool rtos_local_irq_disabled(void)
    
    检查当前中断是否被禁用
    
    返回值：
    -true: 中断已禁用
    -false: 中断未禁用
    
    **FreeRTOS对应接口**: platform_local_irq_disabled()

- 检查是否在中断上下文中::

    bool rtos_is_in_interrupt_context(void)
    
    判断当前代码是否运行在中断服务程序（ISR）中
    
    返回值：
    -true: 在中断上下文中
    -false: 在任务上下文中
    
    **FreeRTOS对应接口**: platform_is_in_interrupt_context()
    
    使用场景：
    - 编写可以在任务和中断中调用的通用函数
    - 根据上下文选择不同的API调用方式

**临界区接口:**

- 进入临界区::

    uint32_t rtos_enter_critical(void)
    
    进入临界区，禁用中断。临界区可以嵌套使用，内部通过计数器管理嵌套层级。
    实际实现为调用rtos_disable_int()。
    
    返回值：
    -返回进入临界区前的中断状态标志
 
    **FreeRTOS对应接口**: vPortEnterCritical() / taskENTER_CRITICAL()
    
- 退出临界区::

    void rtos_exit_critical(uint32_t int_level)
    
    退出临界区，恢复之前保存的中断状态。必须与rtos_enter_critical()成对使用。
    
    参数说明：
    -int_level: 由rtos_enter_critical()返回的中断状态标志
    
    **FreeRTOS对应接口**: vPortExitCritical() / taskEXIT_CRITICAL()


使用示例::

    #include <os/os.h>

    // 示例1：使用中断禁用保护临界代码
    void update_shared_variable(void)
    {
        uint32_t int_level;
        
        // 禁用中断
        int_level = rtos_disable_int();
        
        // 临界代码段 - 快进快出
        shared_counter++;
        shared_flag = true;
        
        // 恢复中断
        rtos_enable_int(int_level);
    }

    // 示例2：使用临界区保护
    void access_critical_resource(void)
    {
        uint32_t int_level;
        
        // 进入临界区
        int_level = rtos_enter_critical();
        
        // 临界代码段
        critical_resource.data = new_value;
        critical_resource.updated = true;
        
        // 退出临界区
        rtos_exit_critical(int_level);
    }

    // 示例3：上下文感知的函数
    void context_aware_function(void)
    {
        if (rtos_is_in_interrupt_context()) {
            // 在中断中的处理逻辑
            BK_LOGI(NULL, "Running in ISR\r\n");
        } else {
            // 在任务中的处理逻辑
            BK_LOGI(NULL, "Running in task\r\n");
        }
    }

    // 示例4：使用宏简化临界区操作
    void macro_example(void)
    {
        GLOBAL_INT_DECLARATION();
        
        GLOBAL_INT_DISABLE();
        // 临界代码段
        shared_data = new_value;
        GLOBAL_INT_RESTORE();
    }

.. note::

    - **临界区 vs 互斥锁**：
        * 临界区：通过禁用中断实现，保护时间极短的代码段（微秒级）
        * 互斥锁：通过任务调度实现，保护时间较长的代码段（毫秒级），支持优先级继承
    - 临界区中禁用了中断，不能调用任何可能引起任务切换或阻塞的API
    - 临界区代码应该尽可能短小，通常只用于保护几条汇编指令
    - 临界区嵌套使用时，必须保证进入和退出成对调用
    - 推荐使用GLOBAL_INT_DECLARATION()、GLOBAL_INT_DISABLE()、GLOBAL_INT_RESTORE()宏
    - 临界区时间过长会影响系统实时性和中断响应时间
    - 不建议在临界区中调用BK_LOGI等可能耗时的函数

**参考示例代码**
------------------------

完整的OS抽象层使用示例可以在以下路径找到：

- API头文件：``includeos/os.h``
- 任务示例：``components/demos/os/os_thread/os_thread.c``
- 队列示例：``components/demos/os/os_queue/os_queue.c``
- 信号量示例：``components/demos/os/os_sem/os_sem.c``
- 互斥锁示例：``components/demos/os/os_mutex/os_mutex.c``
- 定时器示例：``components/demos/os/os_timer/os_timer.c``
- 事件组示例：``components/demos/os/os_event/os_event_group.c``

这些示例展示了OS抽象层API的典型使用方法，建议开发者参考学习。

**示例代码特点：**

- ``os_thread``: 展示任务创建、删除和线程间协作
- ``os_queue``: 展示队列的创建、发送和接收消息
- ``os_sem``: 展示二值信号量用于任务同步
- ``os_mutex``: 展示互斥锁保护共享资源
- ``os_timer``: 展示单次定时器和周期定时器的使用
- ``os_event``: 展示事件组的多种等待模式和同步机制，包括：
    * 中断中设置事件标志
    * 逻辑与等待（所有事件都满足）
    * 逻辑或等待（任一事件满足）
    * 多任务事件同步点
    * 自动清除和手动清除事件标志




