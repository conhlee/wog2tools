# wog2tools

This repository contains tools for working with World of Goo 2 assets. The current tool available is:

## Image Tool

**Image Tool** is a utility for extracting and creating texture files for World of Goo 2.

### Features:
- **Extract Textures:** Convert `.image` files to standard image formats.
- **Create Image Files:** Generate `.image` files from standard image formats.

### Supported Formats:
- **Input:** `.png`, `.bmp`, `.tga`, `.psd`, `.jpg`
- **Output:** `.png`, `.bmp`, `.tga`, `.jpg`

### Usage:
```bash
imagetool -e <input_image_file> -o <output_image_file>
```
```bash
imagetool -c <input_image_file> -o <output_image_file> [-m <mask_image_file>]
```

### Example Commands:
- Extract a texture:
  ```bash
  imagetool -e ./sample.image -o ./sample.png
  ```
- Create an image file:
  ```bash
  imagetool -c ./sample.png -o ./sample.image
  ```
- Create an image file with a mask:
  ```bash
  imagetool -c ./sample.png -m ./samplemask.png -o ./sample.image
  ```

### License:
This software is licensed under the Apache License 2.0. See the [LICENSE](./LICENSE.txt) file for details.