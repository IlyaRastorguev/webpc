import { Module } from "./adapter";

class WebPEncoder implements IWebPEncoder {
  private api: Api = new Proxy<Api>({} as Api, {
    get: function(api, name) {
      if (!api[name as keyof Api]) {
        return (...args: any) => console.warn("method is unimplemented", args);
      } else return api[name as keyof Api];
    },
  });

  constructor() {
    Module.onRuntimeInitialized = () => {
      console.log("WASM module runtime initialized");
      this.api = {
        getImputMemorySize: Module.cwrap("get_mem_size", "number", [
          "number",
          "number",
        ]),
        allocateMemory: Module.cwrap("allocate_memory", "number", ["number"]),
        deallocateMemory: Module.cwrap("deallocate_memory", "", ["number"]),
        encode: Module.cwrap("encode", "", [
          "number",
          "number",
          "number",
          "number",
        ]),
        encodeGif: Module.cwrap("encode_gif", "", [
          "number",
          "number",
          "number"
        ]),
        getResultMemoryPointer: Module.cwrap("get_output_pointer", "", []),
        getResultMemorySize: Module.cwrap("get_output_size", "", []),
        freeMemory: Module.cwrap("free_result", "", ["number"]),
      };
    };
  }

  private getResult(sourcePointer: number) {
    this.api.deallocateMemory(sourcePointer);
    const outputPointer = this.api.getResultMemoryPointer();
    const outputBufferSize = this.api.getResultMemorySize();
    const resultBuffer = new Uint8ClampedArray(
      Module.HEAP8.buffer,
      outputPointer,
      outputBufferSize,
    );
    const result = new Uint8ClampedArray(resultBuffer);
    this.api.freeMemory(outputPointer);

    return result;
  }

  encodeImageData = (
    buffer: Uint8ClampedArray,
    width: number,
    height: number,
    quality: number = 100,
  ): Uint8ClampedArray => {
    const memSize = this.api.getImputMemorySize(width, height);
    const sourcePointer = this.api.allocateMemory(memSize);

    Module.HEAP8.set(buffer, sourcePointer);
    this.api.encode(sourcePointer, width, height, quality);

    return this.getResult(sourcePointer);
  };


  encodeGifImageData = (
    buffer: Uint8Array,
    size: number,
    lossless: 1 | 0 = 1,
  ) => {
    const sourcePointer = this.api.allocateMemory(size);

    Module.HEAP8.set(buffer, sourcePointer);

    this.api.encodeGif(sourcePointer, size, lossless);

    return this.getResult(sourcePointer);
  };
}

export default new WebPEncoder();
