# unity-native-ctx

C++ context and code for reverse engineering Unity native internals on Android.

## Target
- Unity **2018.4.11f1** — armeabi-v7a
- Source: IDB of `libunity.so`

## Structure

| Directory | Contents |
|---|---|
| `context/` | Reversed pseudocode and notes per Unity module |
| `include/` | C++ headers with reconstructed structs and function signatures |
| `src/` | External memory reading code |

## Contributing context

See `context/README.md` for the function documentation format.
