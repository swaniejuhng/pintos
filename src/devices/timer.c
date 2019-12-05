#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/fixed_point.h"
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;
int64_t first_to_wake = INT64_MAX;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
    void
timer_init (void) 
{
    pit_configure_channel (0, 2, TIMER_FREQ);
    intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
    void
timer_calibrate (void) 
{
    unsigned high_bit, test_bit;

    ASSERT (intr_get_level () == INTR_ON);
    printf ("Calibrating timer...  ");

    /* Approximate loops_per_tick as the largest power-of-two
       still less than one timer tick. */
    loops_per_tick = 1u << 10;
    while (!too_many_loops (loops_per_tick << 1)) 
    {
        loops_per_tick <<= 1;
        ASSERT (loops_per_tick != 0);
    }

    /* Refine the next 8 bits of loops_per_tick. */
    high_bit = loops_per_tick;
    for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
        if (!too_many_loops (high_bit | test_bit))
            loops_per_tick |= test_bit;

    printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
    int64_t
timer_ticks (void) 
{
    enum intr_level old_level = intr_disable ();
    int64_t t = ticks;
    intr_set_level (old_level);
    return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
    int64_t
timer_elapsed (int64_t then) 
{
    return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
    void
timer_sleep (int64_t ticks) 
{
    int64_t start = timer_ticks ();


    ASSERT (intr_get_level () == INTR_ON);
    thread_current()->blocked = true; 
    thread_sleep(start+ticks);
    
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
    void
timer_msleep (int64_t ms) 
{
    real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
    void
timer_usleep (int64_t us) 
{
    real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
    void
timer_nsleep (int64_t ns) 
{
    real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
    void
timer_mdelay (int64_t ms) 
{
    real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
    void
timer_udelay (int64_t us) 
{
    real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
    void
timer_ndelay (int64_t ns) 
{
    real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
    void
timer_print_stats (void) 
{
    printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
    static void
timer_interrupt (struct intr_frame *args UNUSED)
{
    int ready_threads, load_avg, fp_59, fp_60, fp_1, div_59_by_60, div_1_by_60, load_avg_times_2, frac;
    int priority, recent_cpu_div_4, nice_times_2, pri_max_to_fp;
    struct list_elem *e;
    struct thread *t;
    struct list *all_list = get_all_list();

    ticks++;
    
    //if(get_min_wakeup_ticks() <= ticks)
    thread_awake(ticks);

    thread_tick ();

    //#ifndef USERPROG
    if(thread_prior_aging || thread_mlfqs){
        if(thread_current() != get_idle_thread())
            thread_current()->recent_cpu = add_fp_and_int(thread_current()->recent_cpu, 1);

        /* recalculate recent_cpu & load_avg every second */
        
        if(timer_ticks() % TIMER_FREQ == 0) {
            //recalc_LA_RC();
            ready_threads = get_ready_list_size();
            //printf("name: %s\n", thread_name());
            //printf("size: %d\n", ready_threads); 
            if((thread_current() != get_idle_thread()) && (thread_current()->status == THREAD_RUNNING))
                ready_threads++;
            //printf("size: %d\n", ready_threads);
            /* RECALCULATE LOAD_AVG */
            /* load_avg = (59 / 60) * load_avg + (1 / 60) * ready_threads */
            load_avg = get_load_avg();
            fp_59 = conv_int_to_fp(59);
            fp_60 = conv_int_to_fp(60);
            fp_1 = conv_int_to_fp(1);
            div_59_by_60 = div_fp_by_fp(fp_59, fp_60);
            div_1_by_60 = div_fp_by_fp(fp_1, fp_60);
            load_avg = add_fp_and_fp(mul_fp_by_fp(div_59_by_60, load_avg), mul_fp_by_int(div_1_by_60, ready_threads));

            //load_avg = div_fp_by_int(add_fp_and_int(mul_fp_by_int(load_avg,59),ready_threads),60);
            //printf("load_avg: %d\n", load_avg);
            set_load_avg(load_avg);

            /*load_avg = get_load_avg();
            fp_59 = conv_int_to_fp(59);
            fp_60 = conv_int_to_fp(60);
            fp_1 = conv_int_to_fp(1);
            div_59_by_60 = div_fp_by_fp(fp_59, fp_60);
            div_1_by_60 = div_fp_by_fp(fp_1, fp_60);
            load_avg = add_fp_and_fp(mul_fp_by_fp(div_59_by_60, load_avg), mul_fp_by_int(div_1_by_60, ready_threads));
            set_load_avg(load_avg);*/

            /* RECALCULATE RECENT_CPU */
            for(e = list_begin(all_list); e != list_end(all_list); e = list_next(e)) {
                /* recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice */
                t = list_entry(e, struct thread, allelem);
                load_avg_times_2 = mul_fp_by_int(load_avg, 2);
                //load_avg_times_2_plus_1 = add_fp_and_int(load_avg_times_2, 1);
                frac = div_fp_by_fp(load_avg_times_2, add_fp_and_int(load_avg_times_2, 1));
                t->recent_cpu = mul_fp_by_fp(frac, t->recent_cpu);
                t->recent_cpu = add_fp_and_int(t->recent_cpu, t->nice);

                /*frac = div_fp_by_fp(mul_fp_by_int(load_avg, 2), add_fp_and_int(mul_fp_by_int(load_avg, 2), 1));
                frac = mul_fp_by_fp(frac, t->recent_cpu);
                t->recent_cpu = add_fp_and_int(frac, t->nice);*/
            }
        }

        /* recalculate priority every 4th tick*/
        if(timer_ticks() % 4 == 0) {
            //recalc_priority();
            // ?????
            for(e = list_begin(all_list); e != list_end(all_list); e = list_next(e)) {
                /* recent_cpu = (2 * load_avg) / (2 * load_avg + 1) * recent_cpu + nice */
                t = list_entry(e, struct thread, allelem);
                if(t == get_idle_thread()) continue;
                /* recalculates the thread's priority based on the new value */

                pri_max_to_fp = conv_int_to_fp(PRI_MAX);
                //recent_cpu_div_4 = div_fp_by_int(thread_current()->recent_cpu, 4);
                recent_cpu_div_4 = div_fp_by_int(t->recent_cpu, 4);
                nice_times_2 = t->nice * 2;
                // PRI_MAX - (recent_cpu / 4)
                priority = sub_fp_from_fp(pri_max_to_fp, recent_cpu_div_4);
                // PRI_MAX - (recent_cpu / 4) - (nice * 2)
                priority = sub_int_from_fp(priority, nice_times_2);
                priority = conv_fp_to_int_rnd_nearest(priority);

                priority = (priority < PRI_MIN) ? PRI_MIN : priority;
                priority = (priority > PRI_MAX) ? PRI_MAX : priority;
                t->priority = priority;
            }
            intr_yield_on_return();
        }
    }
    //#endif
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
    static bool
too_many_loops (unsigned loops) 
{
    /* Wait for a timer tick. */
    int64_t start = ticks;
    while (ticks == start)
        barrier ();

    /* Run LOOPS loops. */
    start = ticks;
    busy_wait (loops);

    /* If the tick count changed, we iterated too long. */
    barrier ();
    return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
    static void NO_INLINE
busy_wait (int64_t loops) 
{
    while (loops-- > 0)
        barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
    static void
real_time_sleep (int64_t num, int32_t denom) 
{
    /* Convert NUM/DENOM seconds into timer ticks, rounding down.

       (NUM / DENOM) s          
       ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
       1 s / TIMER_FREQ ticks
     */
    int64_t ticks = num * TIMER_FREQ / denom;

    ASSERT (intr_get_level () == INTR_ON);
    if (ticks > 0)
    {
        /* We're waiting for at least one full timer tick.  Use
           timer_sleep() because it will yield the CPU to other
           processes. */                
        timer_sleep (ticks); 
    }
    else 
    {
        /* Otherwise, use a busy-wait loop for more accurate
           sub-tick timing. */
        real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
    static void
real_time_delay (int64_t num, int32_t denom)
{
    /* Scale the numerator and denominator down by 1000 to avoid
       the possibility of overflow. */
    ASSERT (denom % 1000 == 0);
    busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
