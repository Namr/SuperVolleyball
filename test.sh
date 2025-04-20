cd build/
make -j20

ssh root@64.23.207.248 'tmux kill-server'
rsync svb_server root@64.23.207.248:~/svb
ssh root@64.23.207.248 'tmux new-session -d "/root/svb/svb_server"'


./svb_client -c -d &
sleep 1s
./svb_client -j 0 -d &
./svb_client -j 0 -d &
./svb_client -j 0 -d
