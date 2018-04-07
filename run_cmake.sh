cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Unix Makefiles" build
# cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" build
mv ./build/compile_commands.json ./compile_commands.json

