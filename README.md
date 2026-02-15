# sailfish-rsc-c

RuneScape Classic is the original 2001-era RuneScape MMORPG. This project is an
open-source RuneScape Classic client port for Sailfish OS. Create your account
at [https://rsc.vet](https://rsc.vet).

<p align="center">
    <img alt="Select device" src="./screenshot.png?"> &nbsp; &nbsp;
</p>

> [!WARNING]  
> This port was created with heavy utilization of OpenAI Codex.


## Known issues
- While logging in, the login text flickers / changes rapidly
- Wiki lookup does not work when sailjail is enabled
- On-screen keyboard Enter key can become disabled; workaround: `pkill -f maliit-server`

## Build instructions
Release build:

```bash
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
