#!/usr/bin/python3
import matplotlib.pyplot as plt
import pandas as pd
import sys
df = pd.read_csv(sys.argv[1])
fig, ax1 = plt.subplots()

color = 'tab:red'
ax1.set_xlabel('Порядковый номер')
ax1.set_ylabel('Размер группы пакетов', color=color)  # we already handled the x-label with ax1
ax1.plot(tuple(df['Group size']),color=color)


ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis
color = 'tab:blue'
ax2.set_ylabel('diff', color=color)

ax2.plot(tuple(df['diff']),color=color)

plt.show()
