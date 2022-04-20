
./coordinator &

./tsd -i 1 -h 0.0.0.0 -c 9090 -p 9190 -t MASTER &
./tsd -i 2 -h 0.0.0.0 -c 9090 -p 9290 -t MASTER &
./tsd -i 3 -h 0.0.0.0 -c 9090 -p 9390 -t MASTER &

./tsd -i 1 -h 0.0.0.0 -c 9090 -p 9490 -t SLAVE &
./tsd -i 2 -h 0.0.0.0 -c 9090 -p 9590 -t SLAVE &
./tsd -i 3 -h 0.0.0.0 -c 9090 -p 9690 -t SLAVE &

./synchronizer -i 1 -h 0.0.0.0 -c 9090 -p 9790 &
./synchronizer -i 2 -h 0.0.0.0 -c 9090 -p 9890 &
./synchronizer -i 3 -h 0.0.0.0 -c 9090 -p 9990 &

