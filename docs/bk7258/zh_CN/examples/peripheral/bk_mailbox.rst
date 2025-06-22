Mailbox
==========================

:link_to_translation:`en:[English]`

1 功能概述
-------------------------------------

    ::

        Mailbox主要用于核间通信，BK7258实现了多种接口，用户可根据具体业务需求选择使用。主要有以下三种：
        1）mailbox通道API；
		2）MB_IPC API（采用了类似socket的API接口和运行方式）
		3）MB_UART API（模拟带自动流控的UART）


2 接口类型1：mailbox通道使用说明
-------------------------------------
    - mailbox通道API介绍：
        在mailbox_channel.c和mailbox_channel.h 中实现了用于核间通信的最多16个mailbox通道，在需要用新的通道实现核间通信时，先在mailbox_channel.h中定义一条通道的名字（枚举名），需要在两核中同时添加，而且必须是相同的枚举值。
        实现核间通信的代码非常简单，主要关注mb_chnl_write，以及两个通过mb_chnl_ctrl注册的回调函数。
        还有一个特别注意的地方是：用mailbox通道做核间通信时，传递的是本核（源核）的内存指针给另外一个核（目的核），在目的核开cache的情况下，都要对收到的指针进行flush_dcache，确保读到的是最新的数据，参见第2节例子1中的代码。

    ::

        1）mb_chnl_open
            /*
              * open logical chnanel.
              * input:
              *     log_chnl  : logical channel id to open.
              *     callback_param : param passsed to all callbacks.
              * return:
              *     succeed: BK_OK;
              *     failed  : fail code.
              *
              */
            bk_err_t mb_chnl_open(u8 log_chnl, void * callback_param);
            callback_param：在调用用户注册的回调函数时，都会回传这个参数。
			这个参数用户可以根据需要设置，如果上层不需要这个参数，可以传NULL。


        2）mb_chnl_close
            /*
              * close logical chnanel.
              * input:
              *     log_chnl  : logical channel id to close.
              * return:
              *     succeed: BK_OK;
              *     failed  : fail code.
              *
              */
            bk_err_t mb_chnl_close(u8 log_chnl);

        3) mb_chnl_write
            /*
              * write to logical chnanel.
              * input:
              *     log_chnl     : logical channel id to write.
              *     cmd_buf       : command buffer to send.
              * 
              * return:
              *     succeed: BK_OK;
              *     failed  : fail code.
              *
              */
            bk_err_t mb_chnl_write(u8 log_chnl, mb_chnl_cmd_t * cmd_buf);
            把指定格式的缓冲区数据写到mailbox。
            typedef struct
            {
            	mb_chnl_hdr_t	hdr;
            
            	u32		param1;
            	u32		param2;
            	u32		param3;
            } mb_chnl_cmd_t;
            
            typedef union
            {
            	struct
            	{
            		u32		cmd           :  8;
            		u32		state         :  4;
            		u32		Reserved      :  20;	/* reserved for system. */
            	} ;
            	u32		data;
            } mb_chnl_hdr_t;
            hdr字段中，用户只要设置cmd域，其他都保留，不需要区设置。

        4）mb_chnl_ctrl
            /*
              * logical chnanel misc io (set/get param).
              * input:
              *     log_chnl     : logical channel id to set/get param.
              *     cmd          : control command for logical channel.
              *     param      :  parameter of the command.
              *        MB_CHNL_GET_STATUS:   param, (u8 *) point to buffer (one byte in size.) to receive status data.
              *        MB_CHNL_SET_RX_ISR:   param, pointer to rx_isr_callback.
              *        MB_CHNL_SET_TX_CMPL_ISR:   param, pointer to tx_cmpl_isr_callback.
              *        MB_CHNL_WRITE_SYNC:   param, pointer to mb_chnl_cmd_t buffer, write to mailbox synchronously.
              * return:
              *     succeed: BK_OK;
              *     failed  : fail code.
              *
              */
            bk_err_t mb_chnl_ctrl(u8 log_chnl, u8 cmd, void * param);
            
            cmd 是MB_CHNL_GET_STATUS时：param 指向一个字节大小的缓冲区，用于接收一个字节的返回值，
			指示通道是否空闲，0 表示通道空闲，其他值表示通道忙。
			通道空闲时，可以调用 mb_chnl_write 往逻辑通道写数据。
			通道忙时，调用mb_chnl_write写数据会失败，返回BK_ERR_BUSY。
            cmd 是MB_CHNL_SET_RX_ISR，MB_CHNL_SET_TX_CMPL_ISR时： 
            param指向函数，是一个对应的函数指针。这些回调函数的原型如下：
			
                  void  (* chnl_rx_isr)(void *param, mb_chnl_cmd_t *cmd_buf);
				  
			      void  (* chnl_tx_cmpl_isr)(void *param, mb_chnl_ack_t *ack_buf);

            param，就是在通道open时传入的参数callback_param。
            通道收到命令时，调用chnl_rx_isr 回调函数，cmd_buf是收到的命令包，
			格式同对方调用mb_chnl_write时的参数mb_chnl_cmd_t * cmd_buf。
            chnl_rx_isr或者上层应用收到cmd_buf并处理完成后，需要给对方一个确认，
			确认包的格式是mb_chnl_ack_t。
			
            typedef struct
            {
            	mb_chnl_hdr_t	hdr;
            
            	u32		ack_data1;
            	u32		ack_data2;
            	union
            	{
            		u32		ack_data3;
            		u32		ack_state; /* ack_state or ack_data3, depends on applications. */
            	};
            } mb_chnl_ack_t;

            确认包ack_buf中hdr字段中的cmd 就是发送方的cmd，（也就是接收到的cmd_buf包中hdr字段中的cmd）。
            ACK包中的ack_data1、ack_data2、ack_data3/ack_state可以在确认的同时用来传输其他信息，
			其内容来自cmd_buf中的param1、param2、param3。
			也就是接收方可以把cmd_buf当成mb_chnl_ack_t类型的内存缓冲区，并修改ack_data1、ack_data2、ack_data3/ack_state的内容。
			在chnl_rx_isr 函数返回时候，自动把cmd_buf中的内容作为ack_buf送到发送方的 chnl_tx_cmpl_isr中（第二个参数）。
			如果接收方chnl_rx_isr没有任何改动cmd_buf，那么发送方的chnl_tx_cmpl_isr的ack_buf中的ack_data1、ack_data2、ack_data3/ack_state，
			就是发送时候的cmd_buf中的param1、param2、param3。
            
            如果双方约定通过ack_state来指示本次传输的处理状态，那么系统定义了如下3种状态：
            enum
            {
            	ACK_STATE_PENDING = 0x01,	
            	ACK_STATE_COMPLETE, 		
            	ACK_STATE_FAIL, 			
            };
            ACK_STATE_PENDING：表示正在处理该命令，如果双方有共用的缓冲区，该缓冲区正在使用中，不能释放。
            ACK_STATE_COMPLETE：cmd handling is competed, addtional infos in ack_data1 & ack_data2. 如果双方有共用的缓冲区，此时可以释放。
            ACK_STATE_FAIL： cmd failed, addtional infos in ack_data1 & ack_data2. 如果双方有共用的缓冲区，此时可以释放。
            如果双方不用ack_state来指示处理状态，处理方式双方上层应用已经约定好，可以不用ack_state，此时ack_state就是ack_data3。
            ack_data1，ack_data2，ack_data3可以在确认的同时来传输其他信息。
            
            通道发送mb_chnl_write中的命令，并获得对方确认时会调用应用注册的chnl_tx_cmpl_isr回调函数，
			该回调函数返回对方的确认数据，格式是mb_chnl_ack_t，如上所述。
            收到确认后如果双方约定通过ack_state来指示本次传输的处理状态，需要检查ack_state的值。
			此外，发送方还要检查ack 包中的hdr字段，如下，
            #define CHNL_STATE_MASK		0xF
            #define CHNL_STATE_COM_FAIL	0x1	/* NO target, it is an ACK to peer CPU. */
            typedef union
            {
            	struct
            	{
            		u32		cmd          :  8;
            		u32		state         :  4;
            		u32		Reserved      :  20;	/* reserved for system. */
            	} ;
            	u32		data;
            } mb_chnl_hdr_t;
            
            hdr字段中，cmd 就是发送的cmd，state字段占4 bit，最低的bit0为通讯状态指示。如果该逻辑通道的对方，没有应用在使用该逻辑通道，
			那么本次发送的cmd包，就没有应用接收和处理，通讯的底层就在hdr.state指示这个状态，表示通信失败，
			如果此时有缓冲区发送给对方，收到通讯失败的确认后，可以在此刻释放缓冲区。
            hdr.state是逻辑通道的通讯层的状态指示。
            ack_state是逻辑通道的应用层的状态指示。
            
            cmd 是MB_CHNL_WRITE_SYNC时：param 指向mb_chnl_cmd_t 缓冲区, 该数据包直接写到物理通道中，
			如果物理通道不空闲，函数中等待直到通道空闲。这个命令用于系统崩溃时，把一些关键信息直接写出去。



    - mailbox通道代码示例：


    ::

        #include <os/os.h>
        #include <driver/mailbox_channel.h>
        
        #define MOD_TAG   "MB_SAMPLE"
        
        static uint8_t x_chnl_inited = 0;
        
        static volatile uint8_t    tx_in_process = 0;
        static volatile uint8_t    tx_failed = 0;
        
        static beken_mutex_t       x_chnl_mutex = NULL;
        static beken_semaphore_t   x_chnl_tx_sem = NULL;
        
        
        static void x_mailbox_rx_isr(void *param,, mb_chnl_cmd_t *cmd_buf)
        {
        	uint8_t	  chnl_id = (uint8_t)param;
        	uint8_t * data_buf = (uint8_t *)cmd_buf->param1;
        	uint32_t  data_len = cmd_buf->param2;
        
        	if((data_buf != NULL) && (data_len > 0))
        	{
        		#if CONFIG_CACHE_ENABLE
        		flush_dcache(data_buf, data_len);
        		#endif
        		
        		// memcpy(rx_buf, data_buf, data_len);
        		// or
        		// rx_callback(chnl_id, data_buf, data_len)
        	}
        
        	return;
        
        }
        
        static void x_mailbox_tx_cmpl_isr(void *param, mb_chnl_ack_t *ack_buf)
        {
        	uint8_t   chnl_id = (uint8_t)param;
        	
        	if(tx_in_process != 0)
        	{
        		/* tx complete. */
        		tx_in_process  = 0;
        
        		if (ack_buf->hdr.state & CHNL_STATE_COM_FAIL) 
        		{
        			tx_failed = 1;
        		}
        		else
        		{
        			/* communication ok, function is completed. */
        			tx_failed = 0;
        		}
        
        		rtos_set_semaphore(&x_chnl_tx_sem);
        
        		return;
        	}
        
        	/*
        	 *   !!!  FAULT  !!!
        	 */
        	BK_LOGE(MOD_TAG, "Fault in %s,chnl:0x%x, cmd:%d, 0x%x\r\n", __func__, chnl_id, ack_buf->hdr.cmd, ack_buf->ack_data1);
        
        	return;
        
        }
        
        bk_err_t x_mailbox_init(uint8_t chnl_id)
        {
        	bk_err_t		ret_code;
        
        	if(x_chnl_inited)
        		return BK_OK;
        
        	ret_code = rtos_init_mutex(&x_chnl_mutex);
        	if(ret_code != BK_OK)
        	{
        		return ret_code;
        	}
        
        	ret_code = rtos_init_semaphore(&x_chnl_tx_sem, 1);
        	if(ret_code != BK_OK)
        	{
        		rtos_deinit_mutex(&x_chnl_mutex);
        
        		return ret_code;
        	}
        
        	ret_code = mb_chnl_open(chnl_id, (void *)chnl_id);
        	if(ret_code != BK_OK)
        	{
        		rtos_deinit_mutex(&x_chnl_mutex);
        		rtos_deinit_semaphore(&x_chnl_tx_sem);
        
        		return ret_code;
        	}
        
        	mb_chnl_ctrl(chnl_id, MB_CHNL_SET_RX_ISR, (void *)x_mailbox_rx_isr);
        	mb_chnl_ctrl(chnl_id, MB_CHNL_SET_TX_CMPL_ISR, (void *)x_mailbox_tx_cmpl_isr);
        
        	x_chnl_inited = 1;
        
        	return BK_OK;
        }
        
        extern int mb_ipc_cpu_is_power_on(u32 cpu_id);
        
        bk_err_t mailbox_transfer(uint8_t chnl_id, uint8_t cmd, void *data_buf, uint32_t data_size)
        {
        	bk_err_t		ret_code;
        
        	if(x_chnl_inited == 0)
        		return BK_FAIL;
        
        	uint8_t		dst_cpu = GET_DST_CPU_ID(chnl_id);
        
        	if( !mb_ipc_cpu_is_power_on(dst_cpu) )
        	{
        		return BK_FAIL;  // or return other error, to indicate the target CPU is power-off
        	}
        
        	rtos_lock_mutex(&x_chnl_mutex);
        
        	tx_in_process = 1;
        	tx_failed = 1;
        
        	mb_chnl_cmd_t mb_cmd;
        
        	mb_cmd.hdr.cmd = cmd;
        	mb_cmd.param1 = (uint32_t)data_buf;
        	mb_cmd.param2 = data_size;
        	mb_cmd.param3 = 0;
        
        	ret_code = mb_chnl_write(chnl_id, &mb_cmd);
        
        	if (ret_code != BK_OK)
        	{
        		BK_LOGE("%s comm failed\n", __func__);
        		goto transfer_exit;
        	}
        
        	ret_code = rtos_get_semaphore(&x_chnl_tx_sem, BEKEN_WAIT_FOREVER); // 2000);
        
        	if (ret_code != BK_OK)
        	{
        		tx_in_process = 0;
        
        		BK_LOGE("%s wait semaphore failed\n", __func__);
        		goto transfer_exit;
        	}
        
        	if( tx_failed != 0)
        		ret_code = BK_FAIL; // CHNL_STATE_COM_FAIL, target is not initialized.
        
        transfer_exit:
        	
        	rtos_unlock_mutex(&x_chnl_mutex);
        	
        	return ret_code;
        }


3 接口类型2：MB_IPC （类似socket的API接口）
--------------------------------------------

    - MB_IPC API介绍：

        ::

            MB_IPC整体上来说，采用了类似socket的API接口和运行方式，因为设计是基于小系统的核间通信，
		所以设计上相对于socket API的行为做了一些限制和简化修改。（IP:Port）替换为（CPU-ID:Port），用于标识通信的双方。
		
            首先，从最宏观方面，socket通信有连接方式和非连接方式，目前本设计的接口支持了半连接方式。
		也就是这个连接方式有个省略：没有做到3次握手，只是两次握手。
		连接建立后，数据传输时没有超时重发。发送方每个tx-cmd发送出去，接收方收到后必然会回复一个对应rx-rsp（ACK）。
		发送方在指定时间内未收到ACK，不做超时重传。收发接口都有一个timeout参数，用于阻塞等待多久，send接口用于等
		待rsp，recv接口用于等待对端send命令的到达。超时重传留给业务层完成。（在最终的代码实现中已经包含重传机制，
		目前没有打开，因为在SoC内通信中，通讯还是比较可靠的）跨核转发通信导致的mailbox中断次数，比直接用mailbox
		通道通信，中断次数增加一倍，（两核直连的mailbox通信，逻辑通道层顺带接收确认机制，为了屏蔽跨核转发通信细
		节，需要在SOCKET完成确认，增加一次ACK通信）。本设计实现不做超时重传，主要是考虑到SoC内的核间通信失败概率
		应该不多，后续若有必要，可以加入重传，在socket创建时统一设置超时时间和重传次数（而不是在每个接口加这两个
		参数）。
            另外，本设计对连接也不进行检测，也就是发现断连需要靠应用层自己。
            其次，基于连接的socket有listen socket 和connection socket概念，server端listen到有接入，内部新建一个
		独立的connection socket用于通信，通常实现上，也是会新建一个线程和客户端通信，实现高并发的通信。本设计没
		有让server端socket有两个独立的属性，而是一体的。也就是说，对server 端的socket，既是listen socket，又是
		connection socket，对每个连接上的客户端，server 逐个处理收发业务。主要是基于实现简单，而且server 需要连
		接多个client的场景也不普遍。另外就是不生成专门线程，也可以节省资源消耗。目前设计Server支持最多3个client
		的连接，针对3核的情况下，每个核可以有一个client。对server而言，需要关注每个client连接到了哪个socket，目
		前设计选择是使用RxCallbak，提供client当前连接的connection id。TxCallback没有使用，选择使用TxSema的方式来
		同步。对client端而言，可以都用semaphore同步。
           最后是port的概念有点变化，TCP/IP中port表示不同的server业务，同一个port号，不论IP是啥，都是指同一类业
		务，用IP:Port表示由哪个主机来提供该业务。本设计中，port不表示一个特定的业务，但是Port和CPU号两者一起，表
		示由哪个核提供特定的server，类似IP:Port。主要是，本设计基于响应及时性考虑，没有把socket做成动态分配和链
		表管理，预先分配足够多的连接控制块（socket管理结构），每个server 有3个，用于支持3个client的连接，每个
		client有一个。然后直接用port作为socket结构（连接控制块）数组的下标来快速引用。把一些处理放在ISR中做了，
		比如根据port找连接，不放在task处理，可以减少由于系统配置和任务优先级导致的时延不确定性，也不用创建一个
		工作线程来做搜索和分配。同一个port，如果cpu id不同，可以用于不同业务的话，可以减少资源的占用，而且，这
		种SoC 内通信，本来就是个封闭系统，不存在外部CPU协作通信以及动态配置，不强求port和业务类型绑定，对client
		来说只要找到server（CPU:Port）就满足设计要求，我们把CPU:Port 成为server ID或client ID。为方便应用配置使
		用，定义了两个枚举类型，server_id_t 和client_id_t，使用者只要在对应的CPU项下填写server/client名字就可以
		自动生成cpu:port 对，程序引用这个枚举常量。
		参见mb_ipc_port_cfg.h，以及mb_ipc_test.c中server / client task的创建示例。

        这套API因为是基于内存指针的数据交换，而不是数据COPY的方式，所以也有一个好处，没有常见的数据粘连问题，
		每次发送的数据包都是整个传输（类似UDP报文）。

        在 Router层之上，传输层通信已经屏蔽了mailbox的连接细节。在传输层之上Server/Client不关心运行的哪个CPU上，
		也就是server、client也可以运行在同一个CPU上，也可以运行在不同CPU上，支持非直连的CPU上APP 通信。

    - 主要API说明：

        1） u32 mb_ipc_socket(u8 port, void * rx_callback)
            综合了socket和bind 两个接口的工作，创建一个socket（内部预先创建，其实是通过port分配或指定一个，并
			创建相关管理资源，比如TxSema，如果rx_callbak不提供时，还创建RxSema）
            如果port为0，系统会分配一个client port。建议client也总是用固定的port，不要让系统分配。因为，由于多
			种原因client可能会意外停止运行，而设计中不检测连接保持，如果动态分配，下次client再次起来时，可能会
			分配到一个不同的port，导致前一次client意外停止的连接还是继续存在（端口及资源没有释放）。如果总是用
			同样的port，client下次连接时（比如reset后重连）发现同样的port已经有一个连接在了，server端就会内部重
			置这个连接，恢复初始状态，然后继续重用这个连接。（注：最新版本已经把端口分配功能取消了，也就是port
			为0会被认为参数错）

        2）int mb_ipc_connect(u32 handle, u8 dst_cpu, u8 dst_port, u32 time_out)
            连接到对应的server。所有API的time_out参数，如果提供的值小于600ms，那么内部会把它改为600ms。主要是
			考虑到flash擦除时间最大可能会到500-600ms左右，如果正在擦除flash，而超时时间设置的比较短，比如100ms，
			那么可能会不必要的发送多个cmd。该值在代码中有宏可配。
            每个server最多可连3个client。（这个值可配置，如果系统不需要那么多client，可以减少到1或者2，以节省资
			源消耗。这个配置值适用所有的server/client，通常不建议改小，因为有的server可能需要比较多的client）

        3）int mb_ipc_close(u32 handle, u32 time_out)
            和server/client断开连接，通常client发起。

        4）int mb_ipc_send(u32 handle, u8 user_cmd, u8 * data_buff, u32 data_len, u32 time_out)
            如果只是发送一个命令，没有数据参数，可以data_buff=NULL，data_len=0。
            如果没有命令，只有数据，那么user_cmd可以用除0xFF外的任意值，这个取决于通信双方约
            定。（INVALID_USER_CMD_ID 设置为 0xFF，表示非法cmd id）
            以上API，返回0，表示成功，小于0表示出错，其值就是错误码。
            每次传输，最多只能传输0xFFFF 字节，也就是data_len 只有低16 bit 有效。

        5）int mb_ipc_recv(u32 handle, u8 * user_cmd, u8 * data_buff, u32 buff_len, u32 time_out)
            如果不关心cmd，或者没有user_cmd，那么 user_cmd可以=NULL。
            小于0，表示出错，其值就是错误码。
            > 0, 表示收到的数据长度。
            =0，表示收到了0长度的数据包，可能有cmd。如果cmd ！= INVALID_USER_CMD_ID
            就是收到了一个合法的user_cmd，否则就是出错了。

            每个API的返回码如下（第一个为0不是错误码）：

        ::

            enum
            {
            	MB_IPC_TX_OK = 0,
            	/* error codes occurring in call side. */
            	MB_IPC_TX_FAILED,
            	MB_IPC_DISCONNECTED,
            	MB_IPC_INVALID_PORT,
            	MB_IPC_INVALID_HANDLE,
            	MB_IPC_INVALID_STATE,
            	MB_IPC_INVALID_PARAM,
            	MB_IPC_NOT_INITED,
            	MB_IPC_TX_TIMEOUT,
            	MB_IPC_TX_BUSY,
            	MB_IPC_NO_DATA,
            	MB_IPC_RX_DATA_FAILED,
            	/* base error code for router. */
            	/* errors occuring in router layer of tx side or router cpu. */
            	MB_IPC_ROUTE_BASE_FAILED = 0x20,
            
            	/* base error code for API-implementation. */
            	/* errors occuring in destination cpu/port API layer. */
            	MB_IPC_API_BASE_FAILED = 0x30,
            };
			

            每层都有对应的错误码范围。


        以下是客户端的实现模型：
            如果mb_ipc_recv 收到数据长度不为0，需要一直接收。（直到返回的数据长度小于提供的缓冲区长度，
			此时说明数据接收完。或者根据mb_ipc_get_recv_data_len返回的长度接收指定长度的数据）
            示例代码可以参考flash_client.c 和mb_ipc_test.c （mb_ipc_test_client 该函数实现了一个客户端
			和server的交互流程）
		
		
        以下是服务器端的实现模型：
            和常规的socket API实现server相比，少了bind，listen，accept，这几个接口实现的功能都在MB_IPC 
			模块内部实现了，包括connect，disconnect（客户端调mb_ipc_close）。在客户端mb_ipc_send时，服
			务器端的rx-callback会收到通知，包括是哪个客户端在send。这种机制可以不用在server端为每个连接
			创建单独的task，由一个server task管理所有client的连接和通信。Server端收到通知后，向MB_IPC 查
			看是哪个连接（client），发生的事件（目前只支持 client 的send 事件），获取收到数据的长度，然
			后调用recv获取数据，app处理数据后，用send应答客户端。
            示例代码可以参考flash_server.c 和mb_ipc_test.c （mb_ipc_test_svr1该函数实现了一个服务器task，
			rx-callback 的实现可以参照这些样本，client id 0~2）

        还有一个特别注意的地方是：用MB_IPC做核间通信时，如果在数据结构中传递了本核（源核）的内存指针给
		另外一个核（目的核）引用，在目的核开cache的情况下，都要对收到的指针进行flush_dcache，确保读到
		的是最新的数据，参见第flash_server.c和flash_client.c中的代码，而在mb_ipc_test.c实现中，没有在
		发送的数据内容中包含内存指针给对方读，所以不用flush_dcache（类似于传值，其实是MB_IPC内部对
		mb_ipc_send中的data_buff内存指针进行了flush_dcache，确保App 调用mb_ipc_recv时收到的是最新的数据）。

    - MB_IPC 模块配置使用说明:

            本设计支持port号最多63个，从1-63，端口0保留，是个非法/无效的端口号。其中前15个保留给server，后
			面48个给client使用。每个server，最多支持3个client连接。
			实际系统中通常不需要这么多端口和连接，可以根据需要配置，文件中有如下配置项：

        ::

            #define SERVER_PORT_NUM		4   // max reserved server PORT num supported by this design is 15 (1~15).
            
            #define MAX_CONNET_PER_SVR	 	3   // at most 3 clients could connect to one server. max support value is 4.
            
            #define CLIENT_PORT_NUM		12  // max client PORT num supported by this design is 48 (16~63).
            
            #define SERVER_PORT_MIN		1   // port 0 is invaid.
            #define CLIENT_PORT_MIN		16


            当前代码，这些配置项设置如上，每个CPU 最多支持4个server task，每个server task支持3个client连接，
			（对每个server，可以是每个CPU 有一个client）。
            这些配置项的值，对每个CPU 都一样，不支持任何配置项针对每个CPU不同值的设置，因为某个CPU上数值不
			同，其实也影响其他CPU 上的资源分配，为了减少复杂性，要求设置统一。（因为其他CPU可能要代为转发，
			需要有足够的资源。目前资源都是静态分配，不是根据连接数量动态分配）
            涉及跨核交互的API，都有一个超时参数，API内部对这个超时值有最小要求，为600ms。如果传入的参数值
			小于该要求值，那么超时时间会被更改为最小要求，这个值可配置。此外还有两个配置参数：重传次数，两
			次重传之间的间隔时间，如下：

            #define MB_IPC_CMD_TIMEOUT		600  //最新代码已经取消了这个最小超时时间值的要求，但是建议
			client端都用这个值，server端为了加快对client的响应，可以用较小的超时时间值，但是确实会碰到
			因flash擦除导致API超时的情况。具体示例可参考flash server 和Flash client的实现
            #define MB_IPC_RETRY_DELAY		2
            #define MB_IPC_RETRY_MAX		1

            考虑到核间通信还是比较可靠的，目前实际没有重传（传输次数为1）
            超时值最小要求选择600ms。主要是考虑到flash擦除时间最恶劣情况下可能会到500-600ms左右，如果正
			在擦除flash，而超时时间又比较短，比如100ms，那么可能会不必要的发送多个cmd。该值可根据flash的情
			况配置，但是通常不用修改，因为如果flash擦除比较快速，没到超时时间命令就已经完成执行。（由于Flash
			ctrl 没有Erase/Program 的suspend/resume 功能，导致Flash擦除时，整个系统不能执行任何指令，一旦擦
			除结束，立即就进行RTOS的tick补偿，导致许多API还没有机会执行就超时了。）



    - MB_IPC API使用示例

    ::

        1）u32 mb_ipc_socket(u8 port, void * rx_callback)

            描述：
            综合了socket和bind 两个接口的工作，创建一个socket，并绑定到指定的端口号。如果port为0，系统会分配
			一个client port。建议client也总是用固定的port，不要让系统分配。因为，由于多种原因client可能会意外
			停止运行，			而设计中不检测连接保持，如果动态分配，下次client再次起来时，可能会分配到一个
			不同的port，导致前一			次client意外停止的连接还是继续存在（端口及资源没有释放）。如果总
			是用同样的port，client下次连接时（比如reset后重连）发现同样的port已经有一个连接在了，server端就
			会内部重置这个连接，恢复初始状态，然后继续重用这个连接。
            （注：最新版本已经把端口分配功能取消了，也就是port为0会被认为参数错）

            参数：port，
            ipc socket绑定的port口，（IPC socket是内部连接控制块的数据结构），
            rx_callback，
            socket事件通知回调函数，它被中断处理函数调用，所以不能在回调函数中做业务处理，只能做一些通知处理。
			通常server因为支持多个连接，所以需要该回调函数，对client端，该参数建议为NULL。

            返回值：成功时，为socket句柄，后续的socket API 都用该句柄作为参数，以引用内部的ipc socket数据结构。 
            失败时，返回0。
            该函数的原型如下：
            u32 svr_rx_callback(u32 handle, u32 connect_id)
            handle，是连接socket的句柄，
            connect_id，连接到server的某个client，0~(MAX_CONNET_PER_SVR -1)。

        ::

            server示例：
            static u32 svr_rx_callback(u32 handle, u32 connect_id)
            {
            	u32  connect_flag;
            
            	if(connect_id >= SVR_CONNECT_MAX)
            		return 0;
            	connect_flag = 0x01 << connect_id;
            	rtos_set_event_ex(&svr_event, connect_flag);
            	return 0;	
            }
            
            void svr_task(void *param)
            {
            	……
            	handle = mb_ipc_socket (IPC_GET_ID_PORT(TEST_SERVER), svr_rx_callback);
            
            	if(handle == 0)
            	{
            		BK_LOGE("ipc_svr", "create_socket failed.\r\n");
            	}
            	……
            }
    
            Client示例：
            void client_task(void * param)
            {
            	……
            	u32 handle = mb_ipc_socket(IPC_GET_ID_PORT(TEST_CLIENT), NULL);
            	if(handle == 0)
            	{
            		bk_printf("client create socket failed\r\n");
            	}
            	……
            }

        2）int mb_ipc_connect(u32 handle, u8 dst_cpu, u8 dst_port, u32 time_out)

        ::

            描述：
            用于client连接到server，server由dst_cpu:dst_port 指定。
            参数：handle，前一个函数创建的socket 的句柄。
            dst_cpu:dst_port，指定server在哪个CPU上哪个端口。
            time_out，超时时间。在该时间内不能和server建立连接则出错。
            返回值：成功时，为0，失败时 < 0，并且该负值就是错误码。

            示例：
            	int ret_val = mb_ipc_connect(handle, IPC_GET_ID_CPU(TEST_SERVER), IPC_GET_ID_PORT(TEST_SERVER), 500);
            	if(ret_val != 0)
            	{
            		bk_printf("client connect failed %d\r\n", ret_val);
            	}
            
            为了方便client、server分配端口号，定义了两个枚举，只要在里面添加client/server ID就可以
			分配端口号，同时决定了server或client运行在哪个CPU上。
            
            typedef enum
            {
            	//   servers resided in cpu0.
            	CPU0_SERVER_ID_START = IPC_SVR_ID_START(0),
            	TEST_SERVER,    // CPU0上有一个server，端口号为1。
            	
            	//   servers resided in cpu1.
            	CPU1_SERVER_ID_START = IPC_SVR_ID_START(1),
            	
            	//   servers resided in cpu2.
            	CPU2_SERVER_ID_START = IPC_SVR_ID_START(2),
            
            } mb_ipc_svr_id_t;
            
            typedef enum
            {
            #if CONFIG_SYS_CPU0
            	//   clients resided in cpu0
            	CPU0_CLIENT_ID_START = IPC_CLIENT_ID_START(0),
            #endif
            	
            #if CONFIG_SYS_CPU1
            	//   clients resided in cpu1
            	CPU1_CLIENT_ID_START = IPC_CLIENT_ID_START(1),
            	TEST_CLIENT,    // CPU1上有个client，，端口号为16。
            #endif
            	
            #if CONFIG_SYS_CPU2
            	//   clients resided in cpu2
            	CPU2_CLIENT_ID_START = IPC_CLIENT_ID_START(2),
            	TEST_CLIENT,    // CPU2上有个client，，端口号为16。
            #endif
            
            } mb_ipc_client_id_t;
            
            程序中使用这些枚举ID以及相应的宏，就可以获得CPU:PORT，如mb_ipc_connect示例所示。

        3）int mb_ipc_close(u32 handle, u32 time_out)

    ::

        描述：
        用于client/server关闭handle指定的连接，该接口通常由client调用，server基本不会主动关闭连接。
		服务程序退出时，要调用mb_ipc_server_close以关闭所有的连接。
        参数：handle，前一个函数创建的socket 的句柄。
        time_out，超时时间。在该时间内不能关闭连接则出错。
        返回值：成功时，为0，失败时 < 0，并且该负值就是错误码。
        
        示例：
        
        mb_ipc_close(handle, 500);
        
        
        4）int mb_ipc_send(u32 handle, u8 user_cmd, u8 * data_buff, u32 data_len, u32 time_out)

    ::

        描述：
        用于client/server数据发送。发送函数有一个和socket send接口不同的参数user_cmd，方便没有数据
		时可以仅发送一个命令，data_buff=NULL，data_len=0。
		因为业务层主要传输数据指针供另一个核读取数据，而不是数据本身，所以经常有一些握手，不一定需
		要额外的命令参数数据，因此在该API中添加了这个user_cmd参数。如果不需要这个参数 ，可以用除0xFF
		外的任意值，这个取决于通信双方约定。（INVALID_USER_CMD_ID 设置为 0xFF，表示非法user_cmd id）

        参数：handle，mb_ipc_socket函数创建的socket 的句柄。
        user_cmd，如上所述，
        data_buff，传输的数据。
        data_len，数据长度，低16 bit 有效，所以最多0xFFFF 字节。
        time_out，超时时间。在该时间内不能发送成功则出错。
        返回值：成功时，为0，失败时 < 0，并且该负值就是错误码。
        
        示例：
        
        	ret_val = mb_ipc_send(handle, FLASH_CMD_READ, (u8 *)cmd_buff, sizeof(flash_cmd_t), FLASH_OPERATE_TIMEOUT);
        
        	if(ret_val != 0)
        	{
        		(*read_buff) ^= 0x01;   // make crc not match
        		os_free(read_buff);
        		TRACE_E(TAG, "%s @%d, 0x%x send failed!\r\n", __FUNCTION__, __LINE__, handle);
        		return;
        	}
        
        
        5）int mb_ipc_recv(u32 handle, u8 * user_cmd, u8 * data_buff, u32 buff_len, u32 time_out)

    ::

        描述：
        用于client/server接收数据。user_cmd参数的描述参见mb_ipc_send接口，如果不关心cmd，
		或者没有user_cmd，那么 user_cmd可以=NULL。
		该接口一个特别重要的地方需要注意：只有把对方发送的数据都读取了，内部接收管理模块
		才会向发送方回应接收成功，然后发送方认为发送成功。
		如果接收方没把发送方发送的data_len长度的数据读走，发送方一直等待，直到超时。
		每次recv返回数据并处理后，都要再次调用recv，参数timeout为0，直到数据读完。
        
        参数：
		handle，mb_ipc_socket函数创建的socket 的句柄。
        user_cmd，用于接收cmd的缓冲，可以为NULL。
        data_buff，存储接收数据的缓冲区。可以为NULL，如果为NULL，就是清空所有接收到的
		数据，发送方会收到成功通知，并可继续发送数据。
        buff_len，缓冲区长度。可以为0，如果为0，就是清空所有接收到的数据，发送方会收
		到成功通知，并可继续发送数据。
        time_out，超时时间。对client来说在该时间内不能接收成功则出错。
        该参数对server无效，因为server用了rx_callback，有数据到达才会去调用该recv函数。
        返回值：> 0, 表示收到的数据长度。
        =0，表示数据接收完了。
        返回值 >= 0时，如果user_cmd != NULL且 cmd ！= INVALID_USER_CMD_ID，就是收到了
		一个合法的user_cmd。
        失败时 < 0，并且该负值就是错误码，此时user_cmd内容无效。
        返回值0需要特别关注处理，对sever可能收到了一个0长度的数据包，也可能是没收到任
		何数据，此时，可以通过user_cmd判断，如果cmd ！= INVALID_USER_CMD_ID就是收到了
		一个0长度参数数据的命令包，否则就是没收到数据。
        对client来说，如果cmd ！= INVALID_USER_CMD_ID就是收到了一个0字节数据的命令包，
		否则就是没收到数据（超时了）。
        为了帮助区分则两种情况，user_cmd参数建议不要为NULL。
        这个函数的特殊之处是：在函数返回值和 user_cmd两处都有数据返回，只有两个地方
		返回的值都失败时，才是API 真正的失败。
        
        server示例：
        		int   rem_len = mb_ipc_get_recv_data_len(handle);
        		
        		bk_printf("==recv 0x%x, %d bytes.\r\n", handle, rem_len);
        
        		u8       data_buff[32];
        		int      read_len = 0;
        
        		if(rem_len == 0)
        		{
        			u8       user_cmd;
        			read_len = mb_ipc_recv(handle, &user_cmd, data_buff, 0, 0);
        			if(read_len < 0)
        			{
        				bk_printf("==recv cmd failed! %d\r\n", read_len);
        			}
        			else
        			{
        				bk_printf("==recv cmd= %d\r\n", user_cmd);
        			}
        		}
        
        		while(rem_len > 0)
        		{
        			read_len = mb_ipc_recv(handle, NULL, data_buff, sizeof(data_buff), 0);
        
        			if(read_len < 0)
        			{
        				bk_printf("==recv failed! %d\r\n", read_len);
        				break;
        			}
        			else if(read_len > 0)
        			{
        				rem_len -= read_len;
        				bk_printf("==> recv %d bytes\r\n", read_len);
        				for(int i = 0; i < read_len; i++)
        				{
        					BK_LOG_RAW("%02x ", data_buff[i]);
        				}
        			
        				if(read_len < sizeof(data_buff))
        				{
        				//	bk_printf("recv complete!\r\n");
        					break;
        				}
        			}
        			else
        			{
        			//	bk_printf("recv failed!\r\n");
        				break;
        			}
        			
        		}
        
        
        Client示例：
        
        		u8       data_buff[32];
        		int      read_len = 0;
        		u8       user_cmd;
        
        		u32      time_out = 200;
        
        		do
        		{
        			read_len = mb_ipc_recv(handle, &user_cmd, data_buff, sizeof(data_buff), time_out);
        
        			if(read_len < 0)
        			{
        				bk_printf("--recv failed! %d\r\n", read_len);
        				break;
        			}
        			else if(read_len == 0)
        			{
        				// if it is the first loop!
        				if((user_cmd == INVALID_USER_CMD_ID) && (time_out != 0)) 
        				{
        					bk_printf("--recv cmd failed!\r\n");
        				}
        
        				bk_printf("--recv complete! cmd= %d\r\n", user_cmd);
        				break;
        			}
        			else // (read_len > 0)
        			{
        				bk_printf("--recv %d bytes\r\n", read_len);
        				for(int i = 0; i < read_len; i++)
        				{
        					BK_LOG_RAW("%02x ", data_buff[i]);
        				}
        			}
        			
        			time_out = 0;   // time_out = 0 for subsequent loops!
        			
        		} while(read_len == sizeof(data_buff));
        
        
        6）int mb_ipc_get_recv_data_len(u32 handle)

    ::

        描述：
        获取当前接收到的待处理数据，该接口通常由server调用。
        参数：handle，mb_ipc_socket函数创建的socket 的句柄。
        返回值：成功时，>=0，指示有多少数据待接收处理。
        失败时 < 0，并且该负值就是错误码。
        对于返回值为0的情况，需要特别关注，建议后续recv时用user_cmd 参数来帮助区
		分是否有数据收到。参见mb_ipc_recv描述。
        
        示例：
        
        参见 mb_ipc_recv的server示例。
        
        7）int mb_ipc_get_recv_event(u32 handle, u32 * event_flag)

    ::

        描述：
        获取当前收到的事件（IPC命令），该接口通常由server调用。Client通常不需要
		提供rx_callback，都是同步API。
        参数：handle，mb_ipc_socket函数创建的socket 的句柄。
             event_flag， 用于保存收到的命令码。
        返回值：成功时，=0，event_flag 中包含当前收到的IPC 命令。
        失败时 < 0，并且该负值就是错误码。
        
        示例：
        	u32  cmd_id;
        	int ret_val = mb_ipc_get_recv_event(handle, &cmd_id);
        	if(ret_val != 0)  // failed
        	{
        		bk_printf("get evt fail %x %d.\r\n", handle, ret_val);
        		return;
        	}
        
        	if(cmd_id > MB_IPC_CMD_MAX)
        	{
        		bk_printf("cmd-id error %d.\r\n", cmd_id);
        		return;
        	}
        	
        	if(cmd_id == MB_IPC_DISCONNECT_CMD)
        	{
        		bk_printf("disconnect 0x%x.\r\n", handle);
        	}
        	else if(cmd_id == MB_IPC_CONNECT_CMD)
        	{
        		bk_printf("connect 0x%x.\r\n", handle);
        	}
        	else if(cmd_id == MB_IPC_SEND_CMD)
        	{
        		…………
        	}
        
        
        
        8）int mb_ipc_server_close(u32 handle, u32 time_out)

    ::

        参见 mb_ipc_close 描述，该API只能被server调用，用于关闭所有的连接。
        示例：
        	mb_ipc_server_close(handle, 1000);
        
        9）u32 mb_ipc_server_get_connect_handle(u32 handle, u32 connect_id)

    ::

        描述：
        该接口通常由server调用，根据rx_callback收到的connect_id 找到和server连
		接的对应的socket句柄，类似socket接口中，由listen socket通过accept生成
		connect socket。
        参数：handle，mb_ipc_socket函数创建的server socket 的句柄。
             connect_id， rx_callback中收到的连接指示，说明client来自哪条连接。
        返回值：返回和client连接的socket句柄，
        失败时，=0，非法句柄，如果传入的handle 或者connect_id不合法，那么就会返
		回非法句柄0。
        成功时 > 0，返回连接句柄，用于后续和client通信。
        
        示例：：
        		connect_handle = mb_ipc_server_get_connect_handle(handle, i);
        
        
        10）异常码解释。

    ::

        // server端需要处理这些命令，client端不关心。
        enum
        {
        	MB_IPC_CONNECT_CMD = 0,
        	MB_IPC_DISCONNECT_CMD,
        
        	MB_IPC_SEND_CMD,
        
        	MB_IPC_CMD_MAX  = 0x7F,  /* cmd id can NOT great than 0x7F. */
        };
        
        enum
        {
        	MB_IPC_TX_OK = 0,      // 发送成功，不是错误码。
        
        	/* error codes for occuring in tx side. */
        	MB_IPC_TX_FAILED,      // mailbox逻辑通道发送失败。
        	MB_IPC_DISCONNECTED,  // 数据在等待发送过程中，链路断开，收到了disconnect命令。
        	MB_IPC_INVALID_PORT,   // 由mb_ipc_connect返回，参数port不是一个server port。
        	MB_IPC_INVALID_HANDLE,  
        // 任一个API 都可能返回，
        // 通过handle 参数没法找到对应的socket连接时，会返回该异常码。
        // 可能连接已经关闭，可能handle值本来就是一个异常值，
        // 可能不是由mb_ipc_socket返回的句柄。
        
        	MB_IPC_INVALID_STATE,  
        // 状态非法，对connect来说，就是已经建立了连接，对该句柄再次调用connect。
        // 对其他API来说，就是尚未建立连接，就进行收发API调用。
        	
        	MB_IPC_INVALID_PARAM,  // mb_ipc_get_recv_event返回，event_flag == NULL
        	MB_IPC_NOT_INITED,  // 句柄没有通过mb_ipc_socket初始化，或者已经mb_ipc_close。
        	MB_IPC_TX_TIMEOUT,  
        // 由send，connect，close返回，相关动作未能在指定时间内完成。
        // client 调用recv 超时不会返回该异常码，而是返回0，表示没数据收到。
        // 参见recv 描述。
        
        	MB_IPC_TX_BUSY,  // 由mb_ipc_send返回，表示有一个send正在进行中，尚未收到应答
        	MB_IPC_NO_DATA,  // 由mb_ipc_get_recv_data_len返回，表示当前没有收到数据包。
        	
        	MB_IPC_RX_DATA_FAILED,  
        // 由mb_ipc_recv返回，表示正在处理接收到的cmd_data_buff内存中的内容时，
        // 发送方更改了cmd_data_buff内存中的数据。
        // 可能是由于超时，发送方释放了内存。
        
        	/* base error code for router. */
        	/* errors occuring in router layer of tx side or router cpu. */
        	MB_IPC_ROUTE_BASE_FAILED = 0x20,  
        // 发送过程中出错，mailbox逻辑通道发送成功后，
        // 送到接收方App-impl层前的路由处理出错，APP-impl 层尚未收到数据。
        // 具体错误码，参见  IPC_ROUTE_XXX  错误描述。
        
        	/* base error code for API-implementation. */
        	/* errors occuring in destination cpu/port API layer. */
        	MB_IPC_API_BASE_FAILED = 0x30,   
        // App-impl 处理时出错。具体错误码，参见  IPC_API_IMPL_XXX  错误描述
        };


        enum
        {
        	/* 4 bits for route status, must be in range 0 ~ 15. */
        	IPC_ROUTE_STATUS_OK = 0x00,
        	IPC_ROUTE_QUEUE_FULL,    // 发送队列满，通常是因为mailbox逻辑通道出问题。
        	IPC_ROUTE_UNREACHABLE,  
        // 找不到目标CPU对应的malbox逻辑通道
        // 或者找不到相应的socket连接，数据没法发送到接收方。
        	IPC_ROUTE_UNSOLICITED_RSP,   
        // 发送方已经由于超时退出发送状态了，再收到接收方的应答包，
        // 导致该rsp无效，只能丢弃。
        
        	IPC_ROUTE_UNMATCHED_RSP,   
        // 发送方已经由于超时退出后再次发送了新的命令包，
        // 此时再收到接收方对前一个包的应答包，应答无效，应答包丢弃。
        	IPC_ROUTE_RX_BUSY,  
        // 接收方还在处理前一个数据包，发送方又发了一个新的数据包给接收方。
        //（对发送方，可能前一个数据包已经超时） 。
        };
        
        enum
        {
        	/* 4 bits for api-implementation status, must be in range 0 ~ 15. */
        	IPC_API_IMPL_STATUS_OK = 0x00,
        	IPC_API_IMPL_NOT_INITED,   // 接收方在数据处理时，socket连接被关闭
        	IPC_API_IMPL_RX_BUSY,   
        // 意义同IPC_ROUTE_RX_BUSY，该错误码定义了，发送方不会收到该错误码，
        // 因为还没交接收方的APP-impl层处理。
        	IPC_API_IMPL_RX_NOT_CONNECT,  
        // 接收方在数据处理时，socket连接已经被关闭了。
        	IPC_API_IMPL_RX_DATA_FAILED,   
        // 意义同MB_IPC_RX_DATA_FAILED，但是该错误码是给发送方的，
        // 发送方通常也不会收到该错误码，因为此时发方应该已经退出发送状态了。
        };



4 接口类型3：MB_UART 使用说明
-------------------------------------

    - MB_UART 模型:
            模拟出了一个带自动流控的UART。

    - MB_UART API说明

        ::

            默认实现了两个MB_UART，如果需要更多通路，可以根据MB_UART0的设置，再去
			配置相应MB_UART。如果只是需要一个MB_UART，可以在此删除MB_UART1，或者用配置宏开关。
            enum
            {
            	MB_UART0 = 0,
            	MB_UART1,
            	MB_UART_MAX,
            };
            
            MB_UART的接收状态：
            #define MB_UART_STATUS_OVF			0x01
            #define MB_UART_STATUS_PARITY_ERR	0x02
            typedef void (* mb_uart_isr_t)(void * param);

    ::

        1） bk_mb_uart_dev_init
        /**
         * @brief	  initialize the mailbox UART device.
         *
         * Initialize the mailbox-emulated UART.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - BK_OK: initialized success.
         *  - other: failed, fail code.
         */
        bk_err_t bk_mb_uart_dev_init(u8 id);
        
        2） bk_mb_uart_dev_deinit
        /**
         * @brief	  de-initialize the mailbox UART device.
         *
         * De-initialize the mailbox-emulated UART.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - BK_OK: initialized success.
         *  - other: failed, fail code.
         */
        bk_err_t bk_mb_uart_dev_deinit(u8 id);

        3） bk_mb_uart_register_rx_isr
        /**
         * @brief	  register rx ISR callback to the mailbox UART device.
         *
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @isr    rx isr, prototype is void (* mb_uart_isr_t)(void * param)
         *
         * @param  will be passed to rx isr callback.
         *
         * @attention   isr callback is called in interrupt disabled context, when received data.
         *
         * @return
         *  - BK_OK: success.
         *  - other: failed, fail code.
         */
        bk_err_t bk_mb_uart_register_rx_isr(u8 id, mb_uart_isr_t isr, void *param);

        4） bk_mb_uart_register_tx_isr
        /**
         * @brief	  register tx ISR callback to the mailbox UART device.
         *
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @isr    tx isr, prototype is void (* mb_uart_isr_t)(void * param)
         *
         * @param  will be passed to tx isr callback.
         *
         * @attention   isr callback is called in interrupt disabled context, after sent cmpelete.
         *
         * @return
         *  - BK_OK: success.
         *  - other: failed, fail code.
         */
        bk_err_t bk_mb_uart_register_tx_isr(u8 id, mb_uart_isr_t isr, void *param);

        5） bk_mb_uart_write_ready
        /**
         * @brief	  get free FIFO space in UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - free FIFO space size in byte.
         */
        u16 bk_mb_uart_write_ready(u8 id);
        
        6） bk_mb_uart_read_ready
        /**
         * @brief	  get data bytes in FIFO of UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - data bytes ready in mailbox-UART FIFO.
         */
        u16 bk_mb_uart_read_ready(u8 id);
        
        7） bk_mb_uart_write_byte
        /**
         * @brief	  send one byte data via UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         * @data   data byte to be sent.
         *
         * @return
         *  - data bytes sent via mailbox-UART (1 or 0).
         */
        u16 bk_mb_uart_write_byte(u8 id, u8 data);
        
		8） bk_mb_uart_write
        /**
         * @brief	  send data in buffer via UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         * @data_buf   data to be sent.
         * @data_len   data bytes in buffer.
         *
         * @return
         *  - data bytes sent via mailbox-UART (0 ~ data_len).
         */
        u16 bk_mb_uart_write(u8 id, u8 *data_buf, u16 data_len);
        
		9） bk_mb_uart_read_byte
        /**
         * @brief	  receive one byte data via UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         * @data   data buffer to receive one byte.
         *
         * @return
         *  - data bytes received via mailbox-UART (1 or 0).
         */
        u16 bk_mb_uart_read_byte(u8 id, u8 * data);

        10） bk_mb_uart_read
        /**
         * @brief	  receive data via UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         * @data_buf   used to receive data.
         * @buf_len    buffer length.
         *
         * @return
         *  - data bytes received via mailbox-UART (0 ~ buf_len).
         */
        u16 bk_mb_uart_read(u8 id, u8 *data_buf, u16 buf_len);
        
        11） bk_mb_uart_is_tx_over
        /**
         * @brief	  check whether data has been flushed out to UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - 1 : no data is pending in Tx FIFO.
         *  - 0 : there are data pending in Tx FIFO.
         */
        bool bk_mb_uart_is_tx_over(u8 id);

        12） bk_mb_uart_dump
        /**
         * @brief	  flush data in buffer via UART device in sync mode.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         * @data_buf   data to be sent.
         * @data_len   data bytes in buffer.
         *
         * @attention   this API is called in interrupt disabled context.
         *
         * @return
         *  - BK_OK: success.
         *  - other: failed, fail code.
         */
        bk_err_t bk_mb_uart_dump(u8 id, u8 *data_buf, u16 data_len);
        
        该API用于特别目的，客户那边不建议使用。一旦调用该API，API就认为系统处于不
		正常状态，会关中断，通过轮询方式在该UART上发送数据，发送方也不依赖于中断回
		调的通知，而是在一个特定内存上握手通讯，交换状态。

        13）  bk_mb_uart_get_status
        /**
         * @brief	  get status of UART device.
         *
         * @id     mailbox uart ID (MB_UART0, MB_UART1).
         *
         * @return
         *  - uart rx/tx status (0, MB_UART_STATUS_OVF, MB_UART_STATUS_PARITY_ERR, or bitwise of these 2 status.).
         */
        u16 bk_mb_uart_get_status(u8 id);


    - 使用示例

        1）从接口到行为，这个MB_UART都类似于常规的UART设备，除了一点：MB_UART基于
		mailbox的逻辑通道，而逻辑通道的通讯依赖于mailbox硬件中断的通知，因此MB_UART的
		实现依赖于系统的中断和mailbox硬件中断，也就是在系统关中断情况下MB_UART无法工作。
		（尽管MB_UART可以轮询方式使用，那也只是说可以不向MB_UART注册Tx/Rx ISR回调函数，
		而不是真正意义上的关中断轮询方式。）在系统启动时候，或者业务模块启动初始化时
		候，先初始化MB_UART设备

        ::

            bk_mb_uart_dev_init(MB_UART0);


        2）如果工作在中断通知方式下，在业务task的初始化模块中向MB_UART注册Tx/Rx ISR回调函数：

        ::

            struct mb_uart_rx_param
            {
            	u8     uart_id;
            	beken_semaphore_t	rx_sema
            } dev_rx_param;
            struct mb_uart_tx_param
            {
            	u8     uart_id;
            	beken_semaphore_t	tx_sema
            } dev_tx_param;

            bk_mb_uart_register_rx_isr(MB_UART0, debug_mb_uart_rx_callback, &dev_rx_param);
            bk_mb_uart_register_tx_isr(MB_UART0, debug_mb_uart_tx_callback, &dev_tx_param);

        3）回调函数的原型是：
	
        ::

            void (* mb_uart_isr)(void * param)
    
            static void debug_mb_uart_rx_callback(void * param)
            {
            	struct mb_uart_param * mbu_para = (struct mb_uart_rx_param *)param;
            	
            	rtos_set_semaphore(&mbu_para-> rx_sema);  // 通知数据到达
            
            	BK_LOGI(TAG," MB_Uart:%d Rx OK\r\n", mbu_para->uart_id);
            }
    
            static void debug_mb_uart_tx_callback(void * param)
            {
            	struct mb_uart_param * mbu_para = (struct mb_uart_tx_param *)param;
            
            	rtos_set_semaphore(&mbu_para-> tx_sema);  // 通知数据发送完成
            
            	BK_LOGI(TAG," MB_Uart:%d Tx OK\r\n", mbu_para->uart_id);
            }



        业务task 收到数据到达通知后用以下API获知MB_UART状态并接收数据：
	
        ::

            rtos_get_semaphore(&mbu_para-> rx_sema, BEKEN_WAIT_FOREVER);
            
            u8 		test_buf[80];
            temp = bk_mb_uart_get_status(uart_id);
            BK_LOGI(TAG,"m_uart%d state:%d \r\n", uart_id, temp);
            
            temp = bk_mb_uart_read_ready(uart_id);
            BK_LOGI(TAG,"m_uart%d RxFIFO data:%d bytes\r\n", uart_id, temp);
            
            while(temp > 0)
            {
            	rd_len = bk_mb_uart_read(uart_id, test_buf, sizeof(test_buf));
            
            	BK_LOGI(TAG,"rx:%d bytes\r\n", rd_len);
            	// BK_LOGI(TAG,"rx:%s \r\n", test_buf);
            
            	for(i = 0; i < rd_len; )
            	{
            		if(rd_len - i >= 8)
            		{
            			os_printf(" %x %x %x %x %x %x %x %x\r\n", test_buf[i], test_buf[i+1],test_buf[i+2],test_buf[i+3],
            				test_buf[i+4],test_buf[i+5],test_buf[i+6],test_buf[i+7]);
            			i+= 8;
            		}
            		else
            		{
            			os_printf(" %x", test_buf[i]);
            			i++;
            		}
            	}
            	os_printf("\r\n");
            
            	// temp -= rd_len;
            	temp = bk_mb_uart_read_ready(uart_id);
            }


        业务task 收到数据发送完成通知后用以下API继续数据发送：

    ::

        rtos_get_semaphore(&mbu_para-> tx_sema, BEKEN_WAIT_FOREVER);
        do
        {	
        	if (bk_mb_uart_write_ready(uart_id) == 0)  // Tx FIFO has space for new data?
        		rtos_delay_milliseconds(2);
        
        	temp = bk_mb_uart_write(uart_id, data_buf, data_len);
        
        	BK_LOGI(TAG,"m_uart%d tx:%d bytes \r\n", uart_id, temp);
        	data_len -= temp;
        
        } while（data_len > 0）;


        如果不注册Tx/Rx ISR回调，就不会有semaphore通知，MB_UART工作在轮询方式，不建议使用。
        
    - 配置MB_UART

        如果需要定义新的MB_UART，需要向在mailbox_channel.h 中定义一条新的逻辑通道。如果是
		CPU0 和 CPU1 之间通讯，那么需要在CPU0和CPU1 之间同时定义，而且确保在两个CPU中，定
		义的逻辑通道顺序是一样的（也就是物理通道不同，逻辑通道序号一样）。

    ::
        
        /* ======================================================================================= */
        /* ==========================  CPU0 --> Target_CPUs mailbox.   =========================== */
        /* ======================================================================================= */
        #if CONFIG_SYS_CPU0
        
        #define SELF_CPU		MAILBOX_CPU0
        
        
        /* the BIGGER the value, the LOWER the channel priority. */
        enum
        {
        	/* CPU0 --> CPU1 */
        	CP1_MB_LOG_CHNL_START   = CPX_LOG_CHNL_START(SELF_CPU, MAILBOX_CPU1),
        	MB_CHNL_HW_CTRL         = CP1_MB_LOG_CHNL_START,
        	MB_CHNL_LOG,   /* MB_CHNL_LOG should be the LAST one. LOWEST priority. */
        	MB_CHNL_COM,   /* MB_CHNL_COM is default mailbox channel used for audio and video */
        	MB_CHNL_LCD_QSPI,
        	MB_CHNL_DATA,
        	MB_CHNL_UART1,
        	MB_CHNL_UART2,
        	CP1_MB_LOG_CHNL_END,
        	CP1_MB_LOG_CHNL_MAX = (CP1_MB_LOG_CHNL_START + LOG_CHNL_ID_MASK), // max 16 channels.
        };
        
        /* ======================================================================================= */
        /* ==========================  CPU1 --> Target_CPUs mailbox.   =========================== */
        /* ======================================================================================= */
        #if CONFIG_SYS_CPU1
        
        #define SELF_CPU		MAILBOX_CPU1
        
        enum
        {
        	/* CPU1 --> CPU0 */
        	CP0_MB_LOG_CHNL_START   = CPX_LOG_CHNL_START(SELF_CPU, MAILBOX_CPU0),
        	MB_CHNL_HW_CTRL         = CP0_MB_LOG_CHNL_START,
        	MB_CHNL_LOG,   /* MB_CHNL_LOG should be the LAST one. LOWEST priority. */
        	MB_CHNL_COM,   /* MB_CHNL_COM is default mailbox channel used for audio and video */
        	MB_CHNL_LCD_QSPI,
        	MB_CHNL_DATA,
        	MB_CHNL_UART1,
        	MB_CHNL_UART2,
        	CP0_MB_LOG_CHNL_END,
        	CP0_MB_LOG_CHNL_MAX = (CP0_MB_LOG_CHNL_START + LOG_CHNL_ID_MASK), // max 16 channels.
        };
        
        在mb_uart_driver.h中定义uart id：
        enum
        {
        	MB_UART0 = 0,
        	MB_UART1,
        	MB_UART_MAX,
        };
        
        在mb_uart_driver.c 中绑定 uart id 和逻辑通道的关系。
        static const u8  		mb_uart_chnl_id[MB_UART_MAX] = {MB_CHNL_UART1, MB_CHNL_UART2};
        
        在xchg buffer 中为该UART 分配Tx/Rx FIFO， 在mb_chnl_buffer.c中：
        
        static const chnl_buff_cfg_t   chnl_buf_cfg_tbl[] = 
        {
        	{ MB_CHNL_HW_CTRL,  MB_CHNL_CTRL_BUFF_LEN },
        	{ MB_CHNL_LOG,      MB_CHNL_LOG_BUFF_LEN  },
        	{ MB_CHNL_UART1,    MB_CHNL_UART_BUFF_LEN },
        	{ MB_CHNL_UART2,    MB_CHNL_UART_BUFF_LEN },
        };
        按照上述配置，xchg buffer中占用的内存大小是：
        （MB_CHNL_CTRL_BUFF_LEN + MB_CHNL_LOG_BUFF_LEN + MB_CHNL_UART_BUFF_LEN + MB_CHNL_UART_BUFF_LEN ）x 2.
        因为每个通道都有Tx/Rx，所以需要 乘以2。
        
        每个UART在xchg buffer 中占用的内存长度（FIFO len），可以在mb_chnl_buffer.h 中配置。
        
        #define MB_CHNL_CTRL_BUFF_LEN		32
        #define MB_CHNL_LOG_BUFF_LEN		144
        #define MB_CHNL_UART_BUFF_LEN		128
        
        如果需要删除多余的MB_UART，参照上述流程做删除操作。

