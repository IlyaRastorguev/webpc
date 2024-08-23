# Webp-Encoder - crossbrowser webp encoder

It's a WASM wrapper arown [libwebp](https://github.com/webmproject/libwebp.git)

## Changelog

1. Add support for encoding gifs to animated webp

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
          dest: "./",
        },
      ],
    }),
  ],
});
```

Example for webpack > 5 with react-scripts:

```javascript
{
  "devDependencies": {
    "copy-webpack-plugin": "^11.0.0"
  }
}
```

```javascript
const CopyPlugin = require("copy-webpack-plugin");
const path = require("path");

//this setting for webpack 5 and above
module.exports = function override(config, env) {
  console.log(config);
  config.resolve.fallback = {
    fs: false,
    tls: false,
    net: false,
    path: false,
    zlib: false,
    http: false,
    https: false,
    stream: false,
    crypto: false,
  };

  config.plugins.push(
    new CopyPlugin({
      patterns: [
        {
          from: "node_modules/webp-encoder/lib/assets/a.out.wasm",
          to: "",
        },
      ],
    })
  );

  return config;
};
```

Example for webpack 4 and react-scripts:

```javascript
{
  "devDependencies": {
    "copy-webpack-plugin": "5.1.2"
  }
}
```

```javascript
const CopyPlugin = require("copy-webpack-plugin");
const path = require("path");

module.exports = function override(config, env) {
  console.log(config);

  config.plugins.push(
    new CopyPlugin(
      [
        {
          from: "node_modules/webp-encoder/lib/assets/a.out.wasm",
          to: "",
        },
      ],
    )
  );

  return config;
};
```

## Usage

```javascript
import WebPEncoder from "webp-encoder"
import { useEffect, useState } from 'react';

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

async function loadGif(src) {
  const imgBlob = await fetch(src).then((resp) => resp.blob());
  return new Uint8Array(await imgBlob.arrayBuffer());
}

function WebPConverter({ sourceImageUrl }) {
  const [url, setUrl] = useState();
  
  const convertImage = useCallback(() => {
    loadImage(sourceImageUrl).then(data => {
      const imageData = data;
      const convertedImage = WebPEncoder.encodeImageData(
        imageData.data,
        imageData.width,
        imageData.height,
        100
      );
      const F = new File([convertedImage], "test.webp", {
        type: "image/webp",
      })

      setUrl(URL.createObjectURL(F))
    });
  }, [sourceImageUrl])
  
  const convertGif = useCallback(() => {
    loadGif(sourceImageUrl).then(imageData => {
      const convertedGif = WebPEncoder.encodeGifImageData(
        imageData,
        imageData.length,
        1
      );
      
      const F = new File([convertedImage], "test.webp", {
        type: "image/webp",
      })

      setUrl(URL.createObjectURL(F))
    });
  }, [sourceImageUrl])

  return (
    <div>
      <header>
        <button onClick={convertImage}>convert image</button>
        <button onClick={convertGif}>convert gif</button>
        <a
          href={url}
          download="test"
        >
          Download result
        </a>
      </header>
    </div>
  );
}

export default WebPConverter;
```

## API

```javascript
interface IWebPEncoder {
  encodeImageData: (
    buffer: Uint8ClampedArray,
    width: number,
    height: number,
    quality: number,
  ) => Uint8ClampedArray;
  encodeGifImageData: (
    buffer: Uint8Array, // uploaded gif file buffer
    size: number, // length of buffer
    lossless: 1 | 0, // if 1 lossless convertation will be using (output size may be larger than original in some cases)
  ) => Uint8ClampedArray;
}
```
