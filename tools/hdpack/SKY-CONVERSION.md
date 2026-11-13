---
lang: en-GB
---

# Convert replacement sky artwork locally

`convert_sky_pack.py` builds a sky-only BFHD pack from replacement panoramic artwork and an installed Xbox 360 game. It preserves the original sky alpha, blends the horizontal join, generates 13 mip levels and encodes BC3 textures at 4096 × 1024. Wake Island and its tutorial share one texture; four unique entries cover all five levels. HDR lighting cubemaps remain unchanged.

The repository contains the converter and synthetic tests. Supply the game files and replacement artwork separately, with the permissions needed for your use. The example ModDB resources are listed as Proprietary; this tool grants no redistribution rights. Keep game files, source artwork, generated packs and conversion evidence out of Git.

Run these commands from the repository root. Put the installed game in `input/pkg` and unpack the separately obtained sky resources under `input/sky-resources`, retaining the `Hemisphericals Skies` folder. All paths in the example mapping are relative to the current working directory.

```powershell
python -m venv input/hdpack-venv
input/hdpack-venv/Scripts/python.exe -m pip install -r tools/hdpack/requirements.txt
input/hdpack-venv/Scripts/python.exe -m unittest discover -s tools/hdpack -p test_sky_conversion.py
input/hdpack-venv/Scripts/python.exe tools/hdpack/convert_sky_pack.py --source . --game input/pkg --mapping tools/hdpack/sky-mapping.example.json --output input/sky-conversion
```

Use Settings > Graphics > Import HD texture pack to import `input/sky-conversion/bf1943-sky-hd.bfhd`, enable HD textures and restart the game. Importing replaces the currently installed BFHD pack; this converter does not merge it with another pack. Keep the previous pack outside the installation if you need to restore it.

The converter checks the package layout and keys. In-game verification must also show a renderer `HD texture` hit for the relevant sky key, followed by a rendered scene. A log saying that the pack opened is insufficient. Inspect the horizon, orientation and blending on each map before treating a new artwork mapping as verified.
