# Webp-Encoder - crossbrowser webp encoder

It's a WASM wrapper arown [libwebp](https://github.com/webmproject/libwebp.git)

## Installing package

```bash
yarn add webp-encoder
```

```bash
npm i webp-encoder
```

## Modify your bundler config for put needed wasm files in your public dir

Here is example for Vite

```javascript
import { defineConfig } from "vite";
import { viteStaticCopy } from "vite-plugin-static-copy";
import * as path from "path";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    viteStaticCopy({
      targets: [
        {
          src:
            path.resolve(__dirname, "node_modules/webpc/lib/assets") +
            "/*.wasm",
          dest: "./assets/",
        },
      ],
    }),
  ],
});
```
And for WebPack

```javascript
new CopyWebpackPlugin([
  {
    from: path.resolve(__dirname, "node_modules/webpc/lib/assets") + '/*.wasm',
    to: 'assets',
  }
]),
```

## Usage

```javascript
import WebPEncoder from "webp-encoder";

async function loadImage(src) {
  const imgBlob = await fetch(src).then((resp) => resp.blob());
  const img = await createImageBitmap(imgBlob);
  const canvas = document.createElement("canvas");
  canvas.width = img.width;
  canvas.height = img.height;
  const ctx = canvas.getContext("2d");
  ctx.drawImage(img, 0, 0);
  return ctx.getImageData(0, 0, img.width, img.height);
}

const imageData = await loadImage("https://some.url.to.image");

const convertedImage = WebPEncoder.encodeImage(imageData, quality);
const F = new File([convertedImage], "test.webp", {
  type: "image/webp",
});
```

```javascript
interface IWebPEncoder {
  encodeImageData: (imageBuf: ImageData, quality: number) => Uint8ClampedArray;
}
```
