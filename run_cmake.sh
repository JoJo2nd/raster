
cd build/linux
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Ninja" ../..
cd ../../

# cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" build
mv ./build/linux/compile_commands.json ./compile_commands.json

