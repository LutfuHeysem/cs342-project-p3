import subprocess
import time
import csv

NUM_RUNS = 10
MODES = [0, 1]

with open('experiment_data.csv', 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['Run', 'Avoidance_Flag', 'Time_Seconds', 'Deadlock_Detected'])

    for flag in MODES:
        for i in range(NUM_RUNS):
            start = time.time()
            result = subprocess.run(['./app', str(flag)], capture_output=True, text=True)
            end = time.time()
            
            deadlock = "DEADLOCK DETECTED" in result.stdout
            writer.writerow([i + 1, flag, round(end - start, 4), int(deadlock)])

print("Experiments finished. Data saved to experiment_data.csv.")
