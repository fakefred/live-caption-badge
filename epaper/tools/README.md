# Tools

## font.py

To display text on the e-paper, we need a bitmap of each printable ASCII
character in a C file. The vendor code only provides fonts up to size 24,
but we need larger fonts such as 48. The C file can be generated in two
steps.

Assumptions:

- Font is monospace (width of each glyph is equal)
- Width is a multiple of 8

### Generate character grid image

It is hard to believe, but the right tool for this task is an applet on
itch.io, [font2bitmap](https://stmn.itch.io/font2bitmap).

Retro game developers 🤝 Embedded engineers

Needing font bitmaps

Click "From file" and upload a monospace font. Leave the characters as-is.
The "font size" is not equal to the glyph width or height. For a 48 pt
font, the recommended settings are

- Grid width: 32
- Grid height: 60
- Font size: 48

### Convert image to C file

We wrote a Python script (`font.py`) to convert the image into a C file.

Dependencies: Pillow, Jinja

Script usage:

```
python font.py INPUT.png --width WIDTH --height HEIGHT --name NAME -o OUTPUT.c
```

For example, to generate `font48.c` with Fira Code:

```
python font.py firacode48.png --width 32 --height 60 --name Font48 -o ../main/font48.c
```

To use the C file, edit the `../main/CMakeLists.txt` and add the C file:

```
idf_component_register(SRCS "epaper.c"
                            ...
                            "font48.c"
                            ...
                            "ImageData.c" INCLUDE_DIRS ".")
```
