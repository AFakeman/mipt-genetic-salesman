for population in $(seq 10000 10000 100000)
do
(./main 4 $population 10 --file graph.txt | awk '{print $2, $4, $6, $8}' && head -1 stats.txt) | python3 plot.py;
done