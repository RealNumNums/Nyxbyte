# Contributing to Nyxbyte

Thanks for helping Nyxbyte grow.

## Development setup

1. Install Visual Studio 2022 Build Tools with the Desktop C++ workload.
2. Install CMake 3.24 or newer.
3. Configure and build:

   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --parallel
   ```

4. Run `build\Release\Nyxbyte.exe`.

## Pull requests

- Keep each pull request focused on one behavior or fix.
- Build the Release configuration before submitting.
- Preserve the no-telemetry, opt-in permissions model.
- Put new behavior behind `ICompanionBrain` when it does not belong in the
  window or renderer.
- Include a short screen recording for visible animation or interaction work
  when practical.

By contributing, you agree that your contribution is licensed under the
project's MIT License.
