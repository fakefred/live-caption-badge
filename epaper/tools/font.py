from PIL import Image
from jinja2 import Template
import typing
import argparse
import math


def hex_lines_from_image(im: Image) -> list[str]:
    buf = []  # buffer of bytes
    byte_count = math.ceil(im.width / 8)

    for y in range(im.height):
        for x_octet in range(byte_count):
            byte = 0
            bit_count = (im.width % 8) if (x_octet == byte_count - 1) else 8
            for dx in range(bit_count):
                x = x_octet * 8 + dx
                pixel = im.getpixel((x, y))
                if all(pixel):
                    byte |= 1 << (7 - dx)
            buf.append(byte)

    hex_lines = []
    for y in range(im.height):
        bytes_per_row = byte_count
        hex_lines.append(", ".join([f"0x{byte:02x}" for byte in buf[:bytes_per_row]]) + ",")
        del buf[:bytes_per_row]

    return hex_lines


def hash_lines_from_image(im: Image) -> list[str]:
    lines = []
    for y in range(im.height):
        line = ""
        for x in range(im.width):
            pixel = im.getpixel((x, y))
            if all(pixel):
                line += "#"
            else:
                line += " "
        lines.append(line)

    return lines



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Bitmap image file")
    parser.add_argument(
        "-o", "--output", type=argparse.FileType("w"), help="Output C file"
    )
    parser.add_argument("--width", type=int, required=True, help="Width of character")
    parser.add_argument("--height", type=int, required=True, help="Height of character")
    parser.add_argument("--name", required=True, help="Font name")
    args = parser.parse_args()

    grid_image = Image.open(args.input)

    if grid_image.width % args.width != 0:
        print(f"Width of input image is not a multiple of {W}")
        exit(1)

    if grid_image.height % args.height != 0:
        print(f"Height of input image is not a multiple of {H}")
        exit(1)

    total_rows = grid_image.height // args.height
    total_cols = grid_image.width // args.width
    total_chars = total_rows * total_cols

    codepoint = ord(" ")
    chars = []

    for r in range(total_rows):
        for c in range(total_cols):
            left = c * args.width
            up = r * args.height
            im = grid_image.crop((left, up, left + args.width, up + args.height))

            chars.append(
                {
                    "char": chr(codepoint),
                    "hex_lines": hex_lines_from_image(im),
                    "hash_lines": hash_lines_from_image(im),
                }
            )
            codepoint += 1

    c_jinja_file = open("bitmap.c.jinja")
    c_template = Template(c_jinja_file.read())
    c_jinja_file.close()

    c_output = c_template.render(
        {
            "chars": chars,
            "font_name": args.name,
            "char_width": args.width,
            "char_height": args.height,
        }
    )

    if args.output:
        args.output.write(c_output)
        args.output.close()
    else:
        print(c_output)
