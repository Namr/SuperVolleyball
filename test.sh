cd build/
make -j20
./svb_client -c -d &
sleep 1s
./svb_client -j 0 -d &
./svb_client -j 0 -d &
./svb_client -j 0 -d
