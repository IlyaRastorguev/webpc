#include "emscripten.h"
#include "libwebp/src/webp/encode.h"
#include <stdint.h>

struct Result {
  uint8_t* buffer_pointer;
  int buffer_size;
};


EMSCRIPTEN_KEEPALIVE
uint8_t* allocate_memory(int width, int height) {
  return malloc(width * height * 4 * sizeof(uint8_t));
}

EMSCRIPTEN_KEEPALIVE
void deallocate_memory(uint8_t* p) {
  free(p);
}


struct Result result = {};
EMSCRIPTEN_KEEPALIVE
void encode(uint8_t* img_in, int width, int height, float quality) {
  uint8_t* img_out;
  size_t size;

  size = WebPEncodeRGBA(img_in, width, height, width * 4, quality, &img_out);

  result.buffer_pointer = img_out;
  result.buffer_size = size;
}

EMSCRIPTEN_KEEPALIVE
void free_result(uint8_t* result) {
  WebPFree(result);
}

EMSCRIPTEN_KEEPALIVE
uint8_t* get_output_pointer() {
  return result.buffer_pointer;
}

EMSCRIPTEN_KEEPALIVE
int get_output_size() {
  return result.buffer_size;
}
