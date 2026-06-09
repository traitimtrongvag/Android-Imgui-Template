# Android ImGui Template

> Android menu template using Zygisk + ImGui.

[![Build Native Libs](https://github.com/traitimtrongvag/Android-Imgui-Template/actions/workflows/build-native.yml/badge.svg)](https://github.com/traitimtrongvag/Android-Imgui-Template/actions/workflows/build-native.yml)
![Android NDK](https://img.shields.io/badge/NDK-r27-brightgreen?logo=android)
![ImGui](https://img.shields.io/badge/ImGui-1.90-blue?logo=github)
![Zygisk](https://img.shields.io/badge/Zygisk-Module-orange)
![GitHub last commit](https://img.shields.io/github/last-commit/traitimtrongvag/Android-Imgui-Template)

https://github.com/user-attachments/assets/816113d9-611d-4909-a08e-0f4e8ab6d03b

---

### Requirements

- Android NDK r27 (ndk;27.0.12077973)

### Build Locally

```bash
export ANDROID_NDK_HOME=/path/to/ndk/27.0.12077973

$ANDROID_NDK_HOME/ndk-build \
  NDK_PROJECT_PATH=app/src/main \
  APP_BUILD_SCRIPT=app/src/main/jni/Android.mk \
  NDK_APPLICATION_MK=app/src/main/jni/Application.mk
```

The compiled .so files land in app/src/main/libs/.

### Build via GitHub Actions

Push to main or master (or trigger manually from the Actions tab). The workflow installs the NDK, builds all ABIs, and uploads the .so files as a build artifact named native-libs.

---

## Dependencies

| Library | Purpose |
|---|---|
| Dear ImGui | In-game overlay UI framework |
| Dobby | ARM64/ARM function hooking |
| KittyMemory | Android process memory patching |
| And64InlineHook | Alternative ARM64 inline hook |
| Cydia Substrate | Additional hooking backend |
| libcurl + OpenSSL | HTTPS communication for authentication |
| nlohmann/json | JSON parsing |
| cpp-httplib | Single-header HTTP client |
| AY_Obfuscate / oxorany | Compile-time string and constant obfuscation |
| stb_image | PNG/JPEG texture loading |
| Font Awesome 5 + 7 | Icon fonts for the UI |

---

## CI/CD

The included GitHub Actions workflow (.github/workflows/build-native.yml) build supported ABIs `arm64-v8a` on every push and exposes the resulting .so files as a downloadable artifact. No local NDK installation is required for a CI build.

- **Latest Build**: Download from [jniLibs](app/src/main/jniLibs/)
- **Latest Stable Build**: Download from [Releases](https://github.com/traitimtrongvag/Android-Imgui-Template/releases)

---

## Security Notice

This project is intended strictly for **educational and research purposes**.

The author(s) are **not responsible** for any misuse, damage, account bans, legal consequences, or other issues that may arise from using or modifying this template. By using this project, you accept full responsibility for your own actions.

Do not use this template to harm others, violate game terms of service, or engage in any illegal activity.

---

## Support

If you have any questions or issues regarding this project, feel free to open an [Issue](https://github.com/traitimtrongvag/Android-ImGui-Template/issues) or contact me via email at [phucan@tutamail.com](mailto:phucan@tutamail.com).

---

## License

MIT © [Android Imgui Template](LICENSE)
