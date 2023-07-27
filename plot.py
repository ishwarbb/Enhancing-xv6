import matplotlib.pyplot as plt


with open("a.txt") as f:
    s = map(lambda x: [int(x[0]), int(x[1]), int(x[2])], map(lambda x: x.split(" "), f.read().split("\n")))

data = list(s)
procs = 15
start = 0   
plt.yticks([0, 1, 2, 3, 4])
plt.xlabel("Ticks")
plt.ylabel("Queue No")
for pid in range(start, procs):
    plotx = []
    ploty = []
    for line in data:
        if(line[1] == pid):
            plotx.append(line[0])
            ploty.append(line[2])
    plt.plot(plotx, ploty,label=str(pid))

plt.legend()
plt.show()
