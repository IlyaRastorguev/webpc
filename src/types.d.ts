type Api = {
  allocateMemory: (width: number, height: number) => number;
  deallocateMemory: (pointer: number) => void;
  encode: (
    imgBuffer: number,
    width: number,
    height: number,
    quality?: number,
  ) => void;
  getResultMemoryPointer: () => number;
  getResultMemorySize: () => number;
  freeMemory: (pointer: number) => void;
};

interface IWebPEncoder {
  encodeImageData: (
    imageBuf: ImageData,
    quality: number,
  ) => Uint8ClampedArray;
}
