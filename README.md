# Skeletal Animation with Vulkan

[![Watch the video](./docs/video/screen_recording.gif)](./docs/video/screen_recording.mov)

*Video quality is poor after conversion, original at `/docs/video/screen_recording.mov`*

## Running
```sh
git clone https://github.com/skrpov/vulkan-animation --recursive
cd vulkan-animation
cmake -S . -B build -G Ninja
cmake --build build
cd ./shaders
./compile.sh
cd ..
./build/app
```
