trap 'pkill -P $$' INT TERM EXIT
./bin/supervolleyball-server &
emrun client_build/client/supervolleyball-client.html
