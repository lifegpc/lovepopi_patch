# Lovepopi Chinese Patch
## How to Compile
### CMake
```powershell
md build && cd build
cmake -A Win32 ../
cmake --build . --config Release
```
### Meson
```powershell
meson setup builddir --buildtype release -Ddefault_library=static -Db_vscrt=static_from_buildtype -Dauto_features=disabled
cd builddir && meson compile
```
