import math
import re
import sys


NUMBER_RE = re.compile(r"[-+]?(?:\d+\.\d+|\d+|\.\d+)(?:[eE][-+]?\d+)?")


class RunningStats:
    """Online mean/variance via Welford, plus samples for exact percentiles."""

    def __init__(self):
        self.count = 0
        self.mean = 0.0
        self.m2 = 0.0
        self.min_value = float("inf")
        self.max_value = float("-inf")
        self.samples = []

    def update(self, value):
        self.samples.append(value)
        self.count += 1
        if value < self.min_value:
            self.min_value = value
        if value > self.max_value:
            self.max_value = value

        delta = value - self.mean
        self.mean += delta / self.count
        delta2 = value - self.mean
        self.m2 += delta * delta2

    def stddev(self):
        if self.count == 0:
            return float("nan")
        # Population standard deviation (sigma).
        return math.sqrt(self.m2 / self.count)

    def percentile(self, p):
        if self.count == 0:
            return float("nan")
        values = sorted(self.samples)
        k = (self.count - 1) * (p / 100.0)
        floor_index = int(math.floor(k))
        ceil_index = int(math.ceil(k))
        if floor_index == ceil_index:
            return values[floor_index]
        low = values[floor_index]
        high = values[ceil_index]
        weight = k - floor_index
        return low + (high - low) * weight


def format_stats(name, stats):
    return (
        f"{name:<8} | "
        f"Count: {stats.count:<5d} | "
        f"Min: {stats.min_value:.4f} | "
        f"Max: {stats.max_value:.4f} | "
        f"Mean: {stats.mean:.4f} | "
        f"SD: {stats.stddev():.4f} | "
        f"p50: {stats.percentile(50):.4f} | "
        f"p95: {stats.percentile(95):.4f} | "
        f"p99: {stats.percentile(99):.4f}"
    )


def calculate_averages():
    first_stats = RunningStats()
    second_stats = RunningStats()
    label = "wikitext-1 parse"

    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        if "wikitext-" not in line.lower():
            continue

        if ":" not in line:
            continue

        line_label, payload = line.split(":", 1)
        label = line_label.strip() or label

        numbers = [float(m.group(0)) for m in NUMBER_RE.finditer(payload)]
        if len(numbers) < 2:
            continue

        first_stats.update(numbers[0])
        second_stats.update(numbers[1])
        if numbers[1] > 100.0 and (numbers[1] * 2) > numbers[0]:
            print(f"[SLOW SECOND] {line}")
            

    if first_stats.count == 0 or second_stats.count == 0:
        print("No matching lines with two numeric values were found.")
        return

    print(label)
    print(format_stats("First", first_stats))
    print(format_stats("Second", second_stats))

    speedup = first_stats.mean / second_stats.mean if second_stats.mean != 0 else float("inf")
    percent_change = (
        ((second_stats.mean - first_stats.mean) / first_stats.mean) * 100
        if first_stats.mean != 0
        else 0.0
    )
    print(f"Speedup (mean first/second): {speedup:.4f}x")
    print(f"Mean percent change (second vs first): {percent_change:.2f}%")

if __name__ == "__main__":
    calculate_averages()