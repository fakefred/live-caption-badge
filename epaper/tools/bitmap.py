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
            bit_count = 8
            if x_octet == byte_count - 1 and im.width % 8 != 0:
                bit_count = x_octet == byte_count - 1
            for dx in range(bit_count):
                x = x_octet * 8 + dx
                pixel = im.getpixel((x, y))
                if pixel > 127:
                    byte |= 1 << (7 - dx)
            buf.append(byte)

    hex_lines = []
    for y in range(im.height):
        bytes_per_row = byte_count
        hex_lines.append(", ".join([f"0x{byte:02x}" for byte in buf[:bytes_per_row]]) + ",")
        del buf[:bytes_per_row]

    return hex_lines


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Bitmap image file")
    parser.add_argument(
        "-o", "--output", type=argparse.FileType("w"), help="Output C file"
    )
    parser.add_argument("--name", required=True, help="Bitmap name")
    args = parser.parse_args()

    image = Image.open(args.input).convert("L") # grayscale
    hex_lines = hex_lines_from_image(image)

    c_jinja_file = open("bitmap.c.jinja")
    c_template = Template(c_jinja_file.read())
    c_jinja_file.close()

    c_output = c_template.render(
        {
            "bitmap_name": args.name,
            "bitmap_width": image.width,
            "bitmap_height": image.height,
            "hex_lines": hex_lines,
        }
    )

    if args.output:
        args.output.write(c_output)
        args.output.close()
    else:
        print(c_output)

