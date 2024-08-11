type Api = {
  getImputMemorySize: (width: number, height: number) => number;
  allocateMemory: (memSize: number) => number;
  deallocateMemory: (pointer: number) => void;
  encode: (
    imgBuffer: number,
    width: number,
    height: number,
    quality?: number,
  ) => void;
  encodeGif: (imgBuffer: number, size: number, lossless: 1 | 0) => void;
  getResultMemoryPointer: () => number;
  getResultMemorySize: () => number;
  freeMemory: (pointer: number) => void;
};

interface IWebPEncoder {
  encodeImageData: (
    buffer: Uint8ClampedArray,
    width: number,
    height: number,
    quality: number,
  ) => Uint8ClampedArray;
  encodeGifImageData: (
    buffer: Uint8Array,
    size: number,
    lossless: 1 | 0,
  ) => Uint8ClampedArray;
}
