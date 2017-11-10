typedef struct rt_thread* rt_thread_t; //rt_thread_t线程句柄，指向线程控制块的指针

struct rt_thread                       //线程控制块
{
    char name[RT_NAME_MAX];            //对象的名称
    rt_uint8_t type;                   //对象的类型
    rt_uint8_t flags;                  //对象的参数
#ifdef RT_USING_MODULE
    void *module_id                    //线程所在的模块id
#endif
    rt_list_t list;                    //对象链表
    rt_list_t tlist;                   //线程链表
    
    
    /*栈指针及入口*/
    void* sp;           //线程的栈指针
    void* entry;        //线程入口
    void* parameter;    //线程入口参数
    void* stack_addr;   //线程栈地址
    rt_uint16_t stack_size; //线程栈大小
    rt_err_t error;         //线程错误号
    rt_uint8_t stat;        //线程状态

    /*优先级相关域*/
    rt_uint8_t current_priority;            //当前优先级
    rt_uint8_t init_priority;               //初始线程优先级
#if RT_THREAD_PRIORITY_MAX > 32
    rt_uint8_t number;
    rt_uint8_t high_mask;
#endif
    rt_uint32_t number_mask;

#if defined(RT_USING_EVENT)
    /*事件相关域*/
    rt_uint32_t event_set;
    rt_uint8_t event_info;
#endif

    rt_ubase_t init_tick;           //线程初始tick
    rt_ubase_t remaining_tick;      //线程当次运行剩余tick

    struct rt_timer thread_timer;   //线程定时器

    /*当线程退出时，需要执行的清理函数*/
    void (*cleanup)(struct rt_thread *tid);
    rt_uint32_t user_data;          //用户数据


}；

//--------------------------------------------------------------------
//线程状态
RT_THREAD_INIT      //线程初始状态。当线程刚开始创建还没开始运行时就处于这个状态；在这个状态下，线程不参与调度
RT_THREAD_SUSPEND   //挂起态、阻塞态。线程此时被挂起：它可能因为资源不可用而挂起等待；这个状态下，线程不参与调度
RT_THREAD_READY     //就绪态。线程正在运行；或当前线程运行完让出处理器后，操作系统寻找最高优先级的就绪态线程运行
RT_THREAD_RUNNING   //运行态。
RT_THREAD_CLOSE     //线程结束态。

//--------------------------------------------------------------------
//调度器相关接口
void rt_system_scheduler_init(void);        //调度器初始化

void rt_system_scheduler_start(void);       //启动调度器

void rt_scheduler(void);                    //执行调度

void rt_scheduler_sethook(void (*hook)(struct rt_thread* from, struct rt_thread* to));//设置调度器钩子

rt_thread_t rt_thread_create(const char* name,
                            void (*entry)(void* parameter),void* parameter,
                            rt_uint32_t stack_size,
                            rt_uint8_t priority, rt_uint32_t tick);//线程创建



//--------------------------------------------------------------------
//动态线程

//初始化两个动态线程
//拥有共同的入口函数，相同的优先级
//入口参数不同

#include <rtthread.h>

#define THREAD_PRIORITY      25
#define THREAD_STACK_SIZE    512
#define THREAD_TIMESLICE     5

//指向线程控制块的指针
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;
//线程入口
static void thread_entry(void* parameter)
{
    rt_uint32_t count = 0;
    rt_uint32_t no = (rt_uint32_t) parameter;   //获得线程入口参数

    while(1)
    {
        //打印线程计数值输出
        rt_kprintf("thread%d count: %d\n",no, count ++);

        //休眠10个OS Tick
        rt_thread_delay(10);
    }
}

//用户应用入口
int rt_application_init()
{
    //创建线程1
    tid1 = rt_thread_create("t1",thread_entry, (void*)1,      //线程入口是threa_entry,入口参数是1
                            THREAD_STACK_SIZE, THREAD_PRIORITY,
                            THREAD_TIMESLICE);
    if (tid1 != RT_NULL)
        rt_thread_startup(tid1);
    else
        return -1;

    //创建线程2
    tid1 = rt_thread_create("t2",thread_entry, (void*)2,      //线程入口是threa_entry,入口参数是2
                            THREAD_STACK_SIZE, THREAD_PRIORITY,
                            THREAD_TIMESLICE);
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);
    else
        return 0;

}



//------------------------------------------------------------------------------------------
//删除线程
//
//会创建两个线程，在一个线程中删除另一个线程
#include <rtthread.h>

#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE       512
#define THREAD_TIMESLICE        5

/*
 *线程删除(rt_thread_delete)函数仅适合于动态线程，为了在一个线程
 *中访问另一个线程的控制块，所以把线程块指针声明成全局类型以供全局
 *访问
 */
 static rt_thread_t tid1 = RT_NULL, tid2 = RT_NULL;

 //线程1的入口函数
 static void thread1_entry(void* parameter)
 {
     rt_uint32_t count = 0;

     while(1)
     {
         //线程1采用低优先级运行，一直打印计数值
         rt_kprintf("thread count: %d\n", count ++);
     }
 }

 //线程2的入口函数
 static void thread2_entry(void* parameter)
 {
        //线程2拥有较高的优先级，以抢占线程1而获得执行

        //线程2启动后先睡眠10个OS Tick
        rt_thread_delay(10);

        //线程2唤醒后直接删除线程1，删除线程1后，线程1自动脱离就绪程序队列
        rt_thread_delete(tid1);
        tid1 = RT_NULL;

        //线程2继续休眠10个OS Tick然后退出，线程2休眠后应切换到idle线程
        //idle线程将执行真正的线程1控制块和线程栈的删除
        //
        rt_thread_delay(10);
        
        //线程2运行结束后也将自动被删除（线程控制块和线程栈依然在idle线程中释放）
        tid2 = RT_NULL;
 }

 //应用入口
 int rt_application_init()
 {
     //创建线程1
     tid1 = rt_thread_create("t1",                        //线程1的名称是t1
                            thread1_entry， RT_NULL,      //入口是thread1_entry,参数是RT_NULL
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid1 != RT_NULL)    //如果获得线程控制块，启动这个线程
        rt_thread_startup(tid1);
    else
        return -1;

    //创建线程2
    tid1 = rt_thread_create("t2",                        //线程2的名称是t2
                            thread2_entry， RT_NULL,      //入口是thread2_entry,参数是RT_NULL
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid2 != RT_NULL)    //如果获得线程控制块，启动这个线程
        rt_thread_startup(tid2);
    else
        return -1;

    return 0;
 }


 //-----------------------------------------------------------------------------------------

 //初始化静态线程
 //初始化2个静态线程，他们拥有共同的入口函数，但参数不同

 #include <rtthread.h>

 #define THREAD_PRIORITY    25
 #define THREAD_STACK_SIZE  512
 #define THREAD_TIMESLICE   5


 //线程1控制块
 static struct rt_thread thread1;
 //线程1栈
 ALIGN(4)
 static struct rt_uint8_t thread1_stack[THREAD_STACK_SIZE];
 //线程2控制块
 static struct rt_thread thread2;
 //线程2栈
 ALIGN(4)
 static rt_uint8_t thread2_stack[THREAD_STACK_SIZE];

 //线程入口
 static void thread_entry(void* parameter)
 {
     rt_uint32_t count = 0;
     rt_uint32_t no = (rt_uint32_t) parameter;  //获得正确的入口参数

     while(1)
     {
         //打印线程计数值输出
         rt_kprintf("thread%d count: %d\n",no, count ++);
        
         //休眠10个OS Tick
         rt_thread_delay(10);
     }
 }

 //用户应用入口
 int rt_application_init()
 {
     rt_err_t result;

     //初始化线程1
     result = rt_thread_init(&thread1, "t1",            //线程名：t1
                            thread_entry,(void*)1,      //线程的入口是thread_entry,入口参数是2
                            &thread1_stack[0],sizeof(thread1_stack),    //线程栈是thread2_stack
                            THREAD_PRIORITY, 10);  
    if (result == RT_EOK)       //如果返回正确，启动线程2
        rt_thread_startup(&thread2);
    else
        return -1;
        
    return 0;
 }

 //------------------------------------------------------------------------------------------
 //线程脱离

 //会创建两个线程（t1和t2），在t2中会对t1进行脱离操作
 //t1脱离后将不在运行，状态也更改为初始状态

#include <rtthread.h>
#include "tc_comn.h"

//线程1控制块
static struct rt_thread thread1;
//线程1栈
static rt_uint8_t thread1_stack[THREAD_STACK_SIZE];
//线程2控制块
static struct rt_thread thread2;
//线程2栈
static rt_uint8_t thread2_stack[THREAD_STACK_SIZE];

//线程1入口
static void thread1_entry(void* parameter)
{
    rt_uint32_t count = 0;

    while(1)
    {
        //线程1采用低优先级运行，一直打印计数
        rt_kprintf("thread count: %d\n",count ++);
    }
}

//线程2入口
static void thread2_entry(void* parameter)
{
    //线程2拥有较高的优先级，以抢占线程1而获得执行

    //线程2启动后先睡眠10个OS Tick
    rt_thread_delay(10);

    //线程2唤醒后直接执行线程1脱离，线程1将从就绪栈程队列中删除

    rt_thread_detach(&thread1);

    //线程2继续休眠10个OS Tick然后退出

    rt_thread_delay(10);

    //线程2运行结束后也将自动被从就绪队列中删除，并脱离线程队列
    
}

int rt_application_init(void)
{
    rt_err_t result;

    //初始化线程1
    result = rt_thread_init(&thread1,"t1",
                            thread1_entry,
                            RT_NULL,
                            &thread1_stack[0],
                            sizeof(thread1_stack),
                            THREAD_PRIORITY, 10);
    if (result == RT_EOK)
        rt_thread_startup(&thread1);

    //初始化线程2
    result = rt_thread_init(&thread2,"t2",
                            thread2_entry,
                            RT_NULL,
                            &thread2_stack[0],
                            sizeof(thread2_stack),
                            THREAD_PRIORITY, 10);
    if (result == RT_EOK)
        rt_thread_startup(&thread2);
    return 0；
}


//---------------------------------------------------------------------------------------
//线程让出处理器
//
//创建两个相同优先级的线程，它们会通过rt_thread_yield
//接口把处理器相互让给对方执行
//

#include <rtthread.h>
#include "tc_comn.h"

//指向线程控制块的指针
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;
//线程1入口
static void thread1_entry(void* parameter)
{
    rt_uint32_t count = 0;

    while(1)
    {
        //打印线程1的输出
        rt_kprintf("thread1: count = %d\n", count ++);

        //执行yield后应该切换到thread2执行
        rt_thread_yield();
    }
}
//线程2入口
static void thread2_entry(void* parameter)
{
    rt_uint32_t count = 0;

    while(1)
    {
        //打印线程1的输出
        rt_kprintf("thread2: count = %d\n", count ++);

        //执行yield后应该切换到thread2执行
        rt_thread_yield();
    }
}

int rt_application_init(void)
{
    //创建线程1
    tid1 = rt_thread_create("thread",
                            thread1_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid1 != RT_NULL)
    rt_thread_startup(tid1);

    //创建线程2
    tid1 = rt_thread_create("thread",
                            thread2_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);

    return 0;
}


//-----------------------------------------------------------------------------------------
//挂起线程
//
//创建两个动态线程（t1和t2）
//低优先级线程t1在启动后将一直持续运行
//高优先级线程t2在一定时刻后唤醒并挂起低优先级线程
//

#include <rtthread.h>
#include "tc_comm.h"

//指向线程控制块的指针
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;
//线程1入口
static void thread1_entry(void* parameter)
{
    rt_uint32_t count = 0;

    while(1)
    {
        //线程1采用低优先级运行，一直打印计数值
        rt_kprintf("thread count: %d\n", count ++);
    }
}

//线程2入口
static void thread2_entry(void* parameter)
{
    //延时10个OS Tick
    rt_thread_delay(10);

    //挂起线程1
    rt_thread_suspend(tid1);

    //延时10个OS Tick
    rt_thread_delay(10);

    //线程2自动退出
}

int rt_application_init(void)
{
    //创建线程1
    tid1 = rt_thread_create("thread",
                            thread1_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid1 != RT_NULL)
    rt_thread_startup(tid1);

    //创建线程2
    tid1 = rt_thread_create("thread",
                            thread2_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);

    return 0;
}


//-----------------------------------------------------------------------------------------
//唤醒线程
//
//创建两个动态线程（t1和t2）
//低优先级线程t1将挂起自身
//高优先级线程t2将在一定时刻后唤醒低优先级线程
//

#include <rtthread.h>
#include "tc_comm.h"

//指向线程控制块的指针
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;
//线程1入口
static void thread1_entry(void* parameter)
{
    //低优先级线程1开始运行
    rt_kprintf("thread1 startup%d\n");

    //挂起自身
    rt_kprintf("suspend thread self\n");
    rt_thread_suspend(tid1);
    //主动执行线程调度
    rt_schedule();

    //当线程1被唤醒时
    rt_kprintf("thread1 resumed\n");
}

//线程2入口
static void thread2_entry(void* parameter)
{
    //延时10个OS Tick
    rt_thread_delay(10);

    //唤醒线程1
    rt_thread_resume(tid1);

    //延时10个OS Tick
    rt_thread_delay(10);

    //线程2自动退出
}

int rt_application_init(void)
{
    //创建线程1
    tid1 = rt_thread_create("t1",
                            thread1_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid1 != RT_NULL)
    rt_thread_startup(tid1);

    //创建线程2
    tid1 = rt_thread_create("t2",
                            thread2_entry,
                            RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);

    return 0;
}

//-----------------------------------------------------------------------------------------

