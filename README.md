# Super Volleyball

A networked multiplayer clone of the only good [Mario Party minigame](https://www.mariowiki.com/Beach_Volley_Folly).
This project is focused on netcode & client-side prediction. Clients can run at any framerate and stay in sync with a server running at a steady 64 ticks per second.

## Connecting to a Server

A server is currently hosted at supervolleyball.xyz on port 25565. You can host your own server just by running `svb_server`.
Clients automatically connect to https://supervolleyball.xyz, but you can override this by creating a file `server_config.txt` that contains the string `address:port` for the server you'd like to connect to instead.

## Building

Most dependencies are given as submodules. You will need openssl & vulkan in your system path on Linux to build.
For Windows dependencies are given in vcpkg.json (which was the only way I was able to get openssl in the PATH for Windows).
