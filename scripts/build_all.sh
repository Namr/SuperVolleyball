cmake -B server_build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cd server_build
make -j12
sudo make install
cd ..
cmake -B client_build -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DPLATFORM=Web -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DBUILD_TESTING=0
cd client_build
cp ../server_build/core/proto ./core -r
make -j12
