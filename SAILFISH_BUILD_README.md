# Sailfish OS build notes (in-progress)

Commands confirmed so far for a Harbour-safe SDL2/software build.

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
    zypper in -y gcc make pkgconfig SDL2-devel SDL2_image-devel
sfdk engine exec sb2 -t SailfishOS-5.0.0.62-armv7hl.default -m sdk-install -R \
    zypper in -y gcc make pkgconfig SDL2-devel SDL2_image-devel
```

## 3. Run the build inside the SDK
From the workspace root (directory containing `.sfdk/`):

```bash
cd ~/sailfish_projects
sfdk build-shell
```

Inside the build shell (project mounted under `/home/nemo/workspace/rsc-c`):

```bash
cd /home/nemo/workspace/rsc-c
./build-sailfish.sh
exit
```

The `install-root` directory now contains the staged `/usr/bin` and `/usr/share`
tree ready for packaging or manual deployment.


## 4. Copy staged files to a device
Use plain scp/rsync from the host (make sure the IP isn't prefixed with `$`).
Example:

```bash
rsync -av install-root/ defaultuser@192.168.1.169:/home/defaultuser/rsc-c-staging/
```

If USB networking causes hangs, fall back to `sfdk deploy` or run `scp` from
inside `sfdk build-shell`.

## 5. Install on device
SSH into the phone, escalate with `devel-su`, and place the files:

```bash
ssh defaultuser@192.168.1.169

devel-su
cd /home/defaultuser/rsc-c-staging/install-root
./install.sh
exit
exit
```

Launch from the app grid or, to mirror it over SSH and keep orientation hints,
use `invoker --type=generic --desktop-file /usr/share/applications/harbour-rsc-c.desktop /usr/bin/env LD_LIBRARY_PATH=/usr/lib64:/usr/libexec/droid-hybris/system/lib64:/vendor/lib64:/system/lib64 mudclient`.

If the icon doesn't start, capture Sailjail's message using:

```bash
journalctl --user -u mapplauncherd.service -f
```

Tap the icon in another SSH session and note the error line; it tells us
what Sailjail rejected.
