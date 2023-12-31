# Building

This project uses WebAssembly to run the client in the browser, which is nice for the end user but makes building quite painful. Once dependencies are installed, the `scripts` folder should make building less painful. They are the recommended way to build the project and are the source of truth for the build process.

`clean.sh` removes the build dirs, `build_all.sh` builds the project assuming that everything has been cleaned. `rebuild.sh` builds the project assuming that `build_all.sh` was already run and has not be cleaned out. `run_single.sh` starts both the server and hosts the client via emscriptens `emrun` tool. `test.sh` and `quick_test.sh` build and run the project, with quick using rebuild instead of a clean build.

## Dependencies

For WASM support, install Emscripten as described [here](https://emscripten.org/docs/getting_started/downloads.html).

We also need the following pre-compiled packages (listed as apt repositories but can be found elsewhere):

```
sudo apt-get install libglfw3-dev libxcursor-dev libxi libssl-dev
```

### Modification to Cap'n Proto
If you see build errors due to capnp, you may need to apply the following change to the dependency:
```
--- a/c++/src/kj/filesystem-disk-unix.c++
+++ b/c++/src/kj/filesystem-disk-unix.c++
@@ -401,7 +401,7 @@ public:
     // A fallocate() wrapper was only added to Android's Bionic C library as of API level 21,
     // but FALLOC_FL_PUNCH_HOLE is apparently defined in the headers before that, so we'll
     // have to explicitly test for that case.
-#if defined(FALLOC_FL_PUNCH_HOLE) && !(__ANDROID__ && __BIONIC__ && __ANDROID_API__ < 21)
+#if !defined(__EMSCRIPTEN__) && defined(FALLOC_FL_PUNCH_HOLE) && !(__ANDROID__ && __BIONIC__ && __ANDROID_API__ < 21)
     KJ_SYSCALL_HANDLE_ERRORS(
         fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, size)) {
       case EOPNOTSUPP:
```

## Running Locally

To run, you must start up a server and connect with a client. You can locally start a server just by running the executable `supervolleyball-server`. You can run the client in a debug state using `emrun build/client/supervolleyball-client.html`. the script `scripts/run_single.sh` does both of these for you.
