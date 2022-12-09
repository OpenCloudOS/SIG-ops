/***************************************************************************
 * loadavg_tracer:  trace high loadavg and dump R|D state process and stack
 * Author: Curu Wong
 * Copyright: Tencent
 *******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/sched/signal.h>
#include <linux/sched/loadavg.h>
#include <linux/kallsyms.h>

#define CHECK_INTERVAL_NSEC  1*1000000L
#define STACK_MAX_ENTRY 32
#define STACK_STR_LEN 512
#define MAX_DUMP_COUNT 50

#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

static int load_threshold = 10;
module_param(load_threshold, int, 0640);
MODULE_PARM_DESC(load_threshold, "dump task when loadavg1 higher than this");

static int dump_interval = 0;
module_param(dump_interval, int, 0640);
MODULE_PARM_DESC(dump_interval, "interval between two task dump(in second)");

struct hrtimer timer;
static u64 last_dump_time;

static long last_avnrun;

static int __init loadavg_tracer_init(void);
static void __exit loadavg_tracer_exit(void);

unsigned int (*p_stack_trace_save_tsk)(struct task_struct *tsk, unsigned long *store,
                  unsigned int size, unsigned int skipnr);
static void dump_r_d_task(void)
{
    struct task_struct *g, *p;
    unsigned int state;
	unsigned long *entries;
	unsigned int nr_entries;

    char *buf;
    int i=0;

	entries = kmalloc_array(STACK_MAX_ENTRY, sizeof(*entries), GFP_ATOMIC);
	if(!entries)
		return;
    buf = kmalloc(STACK_STR_LEN, GFP_ATOMIC);
    if(buf == NULL){
        return;
    }

    rcu_read_lock();
    for_each_process_thread(g, p) {
        state = p->state;
        if( (state == TASK_RUNNING) || ((state & TASK_UNINTERRUPTIBLE) && !(state & TASK_NOLOAD)) ){
            pr_warning("%c [%03d] %-15s %5d\n", task_state_to_char(p), task_cpu(p), p->comm, task_pid_nr(p));

            if(i < MAX_DUMP_COUNT){
                //print stack
				nr_entries = (*p_stack_trace_save_tsk)(p, entries, STACK_MAX_ENTRY, 0);

                memset(buf, 0, STACK_STR_LEN);
				stack_trace_snprint(buf, STACK_STR_LEN, entries, nr_entries, 0);
                pr_warning("\n%s\n", buf);
                i += 1;
            }
        }
    }
    rcu_read_unlock();
    kfree(buf);
}

enum hrtimer_restart timer_callback(struct hrtimer *timer)
{

    unsigned long avnrun;
    int should_dump = 0;
    u64 now = ktime_get_ns();
    avnrun = avenrun[0] + FIXED_1/200;
    if(LOAD_INT(avnrun) >= load_threshold){
        if(dump_interval){
            should_dump = now - last_dump_time >= dump_interval*NSEC_PER_SEC;
        }else{
            should_dump = last_avnrun != avnrun;
        }

        if(should_dump){
            pr_warning("high loadavg detected: load1 %lu.%02lu >= %u\n",
                    LOAD_INT(avnrun), LOAD_FRAC(avnrun),  load_threshold);
            dump_r_d_task();
            last_dump_time = now;
            pr_warning("task dump cost %llu us\n", (ktime_get_ns() - now)/1000);
        }
    }

    last_avnrun = avnrun;

    hrtimer_forward_now(timer, ns_to_ktime(CHECK_INTERVAL_NSEC));
    return HRTIMER_RESTART;
}

static int __init loadavg_tracer_init(void)
{
	p_stack_trace_save_tsk = (void*)kallsyms_lookup_name("stack_trace_save_tsk");
	if(!p_stack_trace_save_tsk){
		pr_err("can't find kernel function 'stack_trace_save_tsk'\n");
		return -EINVAL;;
	}
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer.function = &timer_callback;
    hrtimer_start(&timer, ns_to_ktime(CHECK_INTERVAL_NSEC), HRTIMER_MODE_REL);

    pr_info("loadavg_tracer loaded\n");
    return 0;
}

static void __exit loadavg_tracer_exit(void)
{
    hrtimer_cancel(&timer);
    pr_info("loadavg_tracer unloaded\n");
}

module_init(loadavg_tracer_init);
module_exit(loadavg_tracer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Curu Wong <prinbra@gmail.com>");
MODULE_DESCRIPTION("trace high loadavg and dump R|D task");
MODULE_VERSION("0.11");
