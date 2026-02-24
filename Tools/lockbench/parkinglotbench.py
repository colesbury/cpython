# Measure the round-trip cost of ParkingLot park + unpark (T_sleep).
#
# Two threads ping-pong via ParkingLot: the waiter parks on an address,
# the waker unparks it, and we measure the time from just before the
# wake to just after the waiter resumes. We also measure the cost of
# the store + unpark call.
#
# Usage: python Tools/lockbench/parkinglotbench.py [options]
#
# Tip: use 'taskset -c X,Y python ...' to measure same-core vs
#      cross-core vs cross-socket costs.

from _testinternalcapi import benchmark_parking_lot
import argparse


def percentiles(samples):
    samples.sort()
    n = len(samples)
    return {
        'min': samples[0],
        'p50': samples[n // 2],
        'mean': sum(samples) / n,
        'p90': samples[int(n * 0.90)],
        'p99': samples[int(n * 0.99)],
        'max': samples[n - 1],
    }


def print_stats(title, stats, n, scale=1):
    print(f"{title} \u2014 {n} samples")
    print("\u2500" * 49)
    print(f"  {'Metric':<12} {'ns':>8}")
    for name in ('min', 'p50', 'mean', 'p90', 'p99', 'max'):
        print(f"  {name:<12} {stats[name] / scale:>8.0f}")


def main():
    parser = argparse.ArgumentParser(
        description="Benchmark ParkingLot park/unpark round-trip latency")
    parser.add_argument("--iterations", type=int, default=10000,
                        help="Number of round-trip iterations (default: 10000)")
    parser.add_argument("--warmup", type=int, default=500,
                        help="Number of warmup iterations (default: 500)")
    args = parser.parse_args()

    print(f"Measuring ParkingLot park/unpark overhead "
          f"({args.iterations} iterations)...\n")

    wait_samples, unpark_samples = benchmark_parking_lot(
        args.iterations, args.warmup)

    wait_stats = percentiles(wait_samples)
    unpark_stats = percentiles(unpark_samples)

    # Combine per-iteration: round-trip = park + unpark
    rt_samples = [w + u for w, u in zip(wait_samples, unpark_samples)]
    rt_stats = percentiles(rt_samples)

    print_stats("Round-trip (park + unpark)", rt_stats, len(rt_samples))
    print()
    print(f"  T_park   \u2248 {wait_stats['p50']:.0f} ns (median), "
          f"{wait_stats['mean']:.0f} ns (mean)")
    print(f"  T_unpark \u2248 {unpark_stats['p50']:.0f} ns (median), "
          f"{unpark_stats['mean']:.0f} ns (mean)")


if __name__ == "__main__":
    main()
