import sys

def calculate_averages():
    firstNum = 0.0
    secNum = 0.0
    count = 0
    # Reading from standard input (stdin)
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
            
        # Filter for the specific lines you want
        if "wikitext-1 parse" in line.lower():
            try:
                # Split the name from the numbers (assuming a ':' or similar separator)
                # Example input: wikitext-1 parse A: 12.5, 14.2, 10.8
                sections = line.split(',')
                parts = sections[0].split(':')
                label = parts[0].strip()
                
                # Convert the remaining string into a list of floats
                raw_numbers = parts[1].replace(',', ' ').replace('ms','').split()
                numbers = [float(n) for n in raw_numbers]
                
                if numbers:
                    count += 1
                    firstNum += numbers[0]
                    secNum += numbers[1]
                else:
                    print(f"No valid numbers found in line: {line}")  # Debugging output
                    
            except (ValueError, IndexError):
                # Skips lines that don't match the expected numeric format
                continue

    avg_first = firstNum / count
    avg_second = secNum / count
    # Show how much faster the second average is compared to the first average
    speedup = avg_first / avg_second if avg_second != 0 else float('inf')
    percent_diff = ((avg_second - avg_first) / avg_first) * 100 if avg_first != 0 else 0
    print(f"{label:<20} | Average First: {avg_first:.4f} | Average Second: {avg_second:.4f} | Speedup: {speedup:.2f}%")

if __name__ == "__main__":
    calculate_averages()