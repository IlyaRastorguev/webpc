import { Module } from "./adapter"

class WebPEncoder implements IWebPEncoder {
  #api: Api = new Proxy<Api>({} as Api, {
    get: function(api, name) {
      if (!api[name as keyof Api]) {
        return (...args: any) => console.warn("method is unimplemented", args);
      } else return api[name as keyof Api];
    },
  });

  constructor() {
    Module.onRuntimeInitialized = () => {
      console.log("WASM module runtime initialized");
      this.#api = {
        allocateMemory: Module.cwrap("allocate_memory", "number", [
          "number",
          "number",
        ]),
        deallocateMemory: Module.cwrap("deallocate_memory", "", ["number"]),
        encode: Module.cwrap("encode", "", [
          "number",
          "number",
          "number",
          "number",
        ]),
        getResultMemoryPointer: Module.cwrap("get_output_pointer", "", []),
        getResultMemorySize: Module.cwrap("get_output_size", "", []),
        freeMemory: Module.cwrap("free_result", "", [
          "number",
        ]),
      };
    };
  }
  encodeImageData = (
    buffer: Uint8ClampedArray,
    width: number,
    height: number,
    quality: number = 100,
  ): Uint8ClampedArray => {
    const sourcePointer = this.#api.allocateMemory(
      width,
      height,
    );

    Module.HEAP8.set(buffer, sourcePointer);
    this.#api.encode(sourcePointer, width, height, quality);
    this.#api.deallocateMemory(sourcePointer);
    const outputPointer = this.#api.getResultMemoryPointer();
    const outputBufferSize = this.#api.getResultMemorySize();
    const resultBuffer = new Uint8ClampedArray(
      Module.HEAP8.buffer,
      outputPointer,
      outputBufferSize,
    );
    const result = new Uint8ClampedArray(resultBuffer);
    this.#api.freeMemory(outputPointer);

    return result
  };
}

export default new WebPEncoder();
