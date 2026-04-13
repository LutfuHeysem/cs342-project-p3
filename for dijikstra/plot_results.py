import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('experiment_data.csv')

avg_times = df.groupby('Avoidance_Flag')['Time_Seconds'].mean()
deadlock_counts = df.groupby('Avoidance_Flag')['Deadlock_Detected'].sum()

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

avg_times.plot(kind='bar', color=['red', 'green'], ax=ax1)
ax1.set_title('Avg Execution Time (Avoidance OFF vs ON)')
ax1.set_ylabel('Seconds')
ax1.set_xticklabels(['OFF (0)', 'ON (1)'], rotation=0)

deadlock_counts.plot(kind='bar', color=['orange', 'blue'], ax=ax2)
ax2.set_title('Total Deadlocks Detected (10 runs)')
ax2.set_ylabel('Count')
ax2.set_xticklabels(['OFF (0)', 'ON (1)'], rotation=0)

plt.tight_layout()
plt.savefig('experiment_plots.png')
print("Plots generated: experiment_plots.png")