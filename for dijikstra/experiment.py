import subprocess
import time
import csv
import os

NUM_RUNS = 10 

with open('experiment_data.csv', 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['Run', 'Avoidance_Flag', 'Time_Seconds', 'Deadlock_Detected'])
    f.flush() 

    for flag in [0, 1]:
        for i in range(NUM_RUNS):
            start = time.time()
            try:
                result = subprocess.run(['./myapp', str(flag)], 
                                        capture_output=True, 
                                        text=True, 
                                        start_new_session=True, 
                                        timeout=30)
                end = time.time()
                deadlock = "deadlocked!" in result.stdout.lower()
                writer.writerow([i + 1, flag, round(end - start, 4), int(deadlock)])
                f.flush()
                print(f"Run {i+1} (Flag {flag}) finished in {round(end - start, 4)}s. Deadlock: {deadlock}")
            except subprocess.TimeoutExpired:
                print(f"Run {i+1} (Flag {flag}) timed out.")