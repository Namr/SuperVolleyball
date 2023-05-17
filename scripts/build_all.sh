cmake -B server_build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cd server_build
make -j12
sudo make install
cd ..
cmake -B client_build -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DPLATFORM=Web
cd client_build
make -j12
