# cfgedit

Simple JSON config file editor built from [RapidJSON](rapidjson) and [Dear ImGUI](imgui).

![gif of cfgedit editing a file](./docs/cfgedit.gif)

[Get it here](https://github.com/nathanrw/cfgedit/releases/download/v0.0.1/cfgedit.exe)

## Building

```Bash
git clone https://github.com/nathanrw/cfgedit
git submodule update --init --recursive
cd cfgedit
mkdir _build
cd _build
cmake ..
cmake --build . --target cfgedit
# or start the IDE project
```

[rapidjson]: https://github.com/Tencent/rapidjson
[imgui]: https://github.com/ocornut/imgui