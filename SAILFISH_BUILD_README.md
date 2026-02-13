# Sailfish OS RPM build notes

These steps cover the RPM build workflow only.

## 1. Install SDK targets on the host
Run once after installing the Sailfish SDK:

```bash
sfdk tools target install SailfishOS-5.0.0.62-aarch64
sfdk tools target install SailfishOS-5.0.0.62-armv7hl
```

## 2. Install toolchain packages inside each target
Use Scratchbox2 (`sb2`) via `sfdk engine exec` so the packages go into the
selected target image:

```bash
sfdk engine exec sb2 -t SailfishOS-5.0.0.62-aarch64.default -m sdk-install -R \
    zypper in -y gcc make pkgconfig SDL2-devel SDL2_image-devel \
    maliit-framework-wayland-devel glib2-devel
sfdk engine exec sb2 -t SailfishOS-5.0.0.62-armv7hl.default -m sdk-install -R \
    zypper in -y gcc make pkgconfig SDL2-devel SDL2_image-devel \
    maliit-framework-wayland-devel glib2-devel
```

### Maliit (on-screen keyboard) notes
- Build requires `pkg-config --cflags maliit-glib` to work.
- `maliit-framework-wayland-devel` provides
  `/usr/include/maliit-2/maliit-glib/maliitinputmethod.h`.
- For the RPM spec, add `BuildRequires: pkgconfig(maliit-glib)` and
  `BuildRequires: pkgconfig(glib-2.0)`.
- Runtime dependency on device: `maliit-framework-wayland-glib`
  (provides `libmaliit-glib.so.2`).
- If OSK shows but Enter stops responding, a device reboot has fixed it.
- Alternative to reboot (often enough):
  - `systemctl --user restart maliit-server.service`
  - `pkill -f maliit-server`

## 3. Build RPM with sfdk
From the project root:

```bash
cd ~/sailfish_projects/rsc-c
sfdk build
```

Optional full rebuild:

```bash
sfdk build -- --define "clean_build 1"
```

Optional debug build:

```bash
sfdk build -- --define "debug_build 1"
```

Optional full rebuild + debug:

```bash
sfdk build -- --define "clean_build 1" --define "debug_build 1"
```

The RPM appears under:

```
RPMS/aarch64/rsc-c-*.aarch64.rpm
```

## 4. Install on device (RPM)
Copy the RPM and install it on the phone:

```bash
scp RPMS/aarch64/rsc-c-*.aarch64.rpm defaultuser@192.168.1.169:/home/defaultuser/
ssh defaultuser@192.168.1.169
devel-su
rpm -Uvh /home/defaultuser/rsc-c-*.aarch64.rpm
```

Launch from the app grid or run `/usr/bin/rsc-c` from a terminal.
