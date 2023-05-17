# Super Volleyball

An online multiplayer clone of the [best Mario Party minigame](https://www.mariowiki.com/Beach_Volley_Folly).

## Building

This project uses WebAssembly to run the client in the browser, which is nice for the end user but makes building quite painful. Most annoying of all, you *must* clean (delete) your build directory if you want to switch from building the client to the server or vice versa.

### Dependencies

For WASM support, install Emscripten as described [here](https://emscripten.org/docs/getting_started/downloads.html).

We also need the following pre-compiled packages (listed as apt repositories but can be found elsewhere):

```
sudo apt-get install libglfw3-dev libxcursor-dev libxi libssl-dev
```


### Building The Client

Assuming emscripten is properly configured and sourced, you can run the below to build the client: 

```
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DPLATFORM=Web
cd build
make
```

### Building The Server

Run the following to build the server. Remember, you must clear away the client build artifacts for this to work properly.

```
cmake -B build
cd build
make
```

A script (`scripts/build_all.sh`) can be used to build both client and server. It also installs the server into the `bin` directory which allows you to run both after the builds have completed.


## Running Locally

To run, you must start up a server and connect with a client. You can locally start a server just by running the executable `supervolleyball-server`. You can run the client in a debug state using `emrun build/client/supervolleyball-client.html`. the script `scripts/run_single.sh` does both of these for you.
