# cfgedit

Simple JSON config file editor.

![gif of cfgedit editing a file](./docs/cfgedit.gif)

Uses RapidJSON for JSON-editing.

Uses Dear ImGUI and GLFW for UI.

## Building

```Bash
git clone https://github.com/nathanrw/cfgedit
cd cfgedit
mkdir _build
cd _build
cmake ..
cmake --build . --target cfgedit
# or start the IDE project
```