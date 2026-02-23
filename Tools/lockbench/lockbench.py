# Measure the performance of PyMutex locks with short critical sections.
#
# Usage: python Tools/lockbench/lockbench.py [options]
#
# How to interpret the results:
#
# Acquisitions (kHz): Reports the total number of lock acquisitions in
# thousands of acquisitions per second. This is the most important metric,
# particularly for the 1 thread case because even in multithreaded programs,
# most locks acquisitions are not contended. Values for 2+ threads are
# only meaningful for `--disable-gil` builds, because the GIL prevents most
# situations where there is lock contention with short critical sections.
#
# Fairness: A measure of how evenly the lock acquisitions are distributed.
# A fairness of 1.0 means that all threads acquired the lock the same number
# of times. A fairness of 1/N means that only one thread ever acquired the
# lock.
# See https://en.wikipedia.org/wiki/Fairness_measure#Jain's_fairness_index

from _testinternalcapi import benchmark_locks
import argparse


def parse_threads(value):
    if '-' in value:
        lo, hi = value.split('-', 1)
        lo, hi = int(lo), int(hi)
        return range(lo, hi + 1)
    return range(int(value), int(value) + 1)

def jains_fairness(values):
    # Jain's fairness index
    # See https://en.wikipedia.org/wiki/Fairness_measure
    return (sum(values) ** 2) / (len(values) * sum(x ** 2 for x in values))

def main():
    parser = argparse.ArgumentParser(description="Benchmark PyMutex locks")
    parser.add_argument("--work-inside", type=int, default=1,
                        help="Amount of work inside the critical section")
    parser.add_argument("--work-outside", type=int, default=0,
                        help="Amount of work outside the critical section")
    parser.add_argument("--acquisitions", type=int, default=1,
                        help="Lock acquisitions per loop iteration")
    parser.add_argument("--total-iters", type=int, default=0,
                        help="Fixed iterations per thread (0 = time-based)")
    parser.add_argument("--bg-threads", type=int, default=0,
                        help="Number of background CPU threads")
    parser.add_argument("--bg-work", type=int, default=1000,
                        help="Amount of work per background thread iteration")
    parser.add_argument("--num-locks", type=int, default=1,
                        help="Number of locks (threads assigned round-robin)")
    parser.add_argument("threads", type=parse_threads, nargs='?',
                        default=range(1, 11),
                        help="Number of threads: N or MIN-MAX (default: 1-10)")
    args = parser.parse_args()

    header = f"{'Threads': <10}{'Acq (kHz)': >12}{'Fairness': >10}"
    if args.total_iters:
        header += f"{'Wall (ms)': >12}"
    header += f"{'Spins': >12}{'Spin %': >10}{'ns/spin': >10}{'Parks': >10}{'Handoffs': >10}"
    if args.bg_threads:
        header += f"{'BG (kHz)': >12}"
    print(header)
    for num_threads in args.threads:
        acquisitions, thread_iters, spin_time_ns, spin_count, \
            park_count, handoff_count, elapsed_ns, bg_iters = \
            benchmark_locks(
                num_threads, args.work_inside, args.work_outside,
                1000, args.acquisitions, args.total_iters,
                args.bg_threads, args.bg_work, args.num_locks)

        wall_ms = elapsed_ns / 1e6
        acquisitions /= 1000  # report in kHz for readability
        fairness = jains_fairness(thread_iters)
        spin_pct = 100.0 * spin_time_ns / elapsed_ns / num_threads
        ns_per_spin = spin_time_ns / spin_count if spin_count else 0

        line = f"{num_threads: <10}{acquisitions: >12.0f}{fairness: >10.2f}"
        if args.total_iters:
            line += f"{wall_ms: >12.1f}"
        line += f"{spin_count: >12}{spin_pct: >9.1f}%{ns_per_spin: >10.0f}"
        line += f"{park_count: >10}{handoff_count: >10}"
        if args.bg_threads:
            bg_rate = bg_iters * 1e6 / elapsed_ns  # kHz
            line += f"{bg_rate: >12.0f}"
        print(line)


if __name__ == "__main__":
    main()
