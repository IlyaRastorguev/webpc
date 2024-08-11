#include "emscripten.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "giflib/lib/gif_lib.h"
#include "libwebp/examples/example_util.h"
#include "libwebp/examples/gifdec.h"
#include "libwebp/examples/unicode.h"
#include "libwebp/examples/unicode_gif.h"
#include "libwebp/imageio/imageio_util.h"
#include "libwebp/src/webp/encode.h"
#include "libwebp/src/webp/mux.h"
#include "libwebp/src/webp/types.h"

#include "giflib/lib/gif_lib_private.h"

#if defined(HAVE_UNISTD_H) && HAVE_UNISTD_H
#include <unistd.h>
#endif

struct Result {
  uint8_t *buffer_pointer;
  int buffer_size;
};

//------------------------------------------------------------------------------

static int transparent_index = GIF_INDEX_INVALID; // Opaque by default.

static const char *const kErrorMessages[-WEBP_MUX_NOT_ENOUGH_DATA + 1] = {
    "WEBP_MUX_NOT_FOUND", "WEBP_MUX_INVALID_ARGUMENT", "WEBP_MUX_BAD_DATA",
    "WEBP_MUX_MEMORY_ERROR", "WEBP_MUX_NOT_ENOUGH_DATA"};

static const char *ErrorString(WebPMuxError err) {
  assert(err <= WEBP_MUX_NOT_FOUND && err >= WEBP_MUX_NOT_ENOUGH_DATA);
  return kErrorMessages[-err];
}

enum {
  METADATA_ICC = (1 << 0),
  METADATA_XMP = (1 << 1),
  METADATA_ALL = METADATA_ICC | METADATA_XMP
};

#define READ(_gif, _buf, _len)                                                 \
  (((GifFilePrivateType *)_gif->Private)->Read                                 \
       ? ((GifFilePrivateType *)_gif->Private)->Read(_gif, _buf, _len)         \
       : fread(_buf, 1, _len, ((GifFilePrivateType *)_gif->Private)->File))

//------------------------------------------------------------------------------
//

GifFileType *GifFromUint8Array(uint8_t *image_buf, int size, int *Error) {
  GifFileType *Gif = NULL;
  FILE *tempFile = tmpfile();

  if (tempFile == NULL) {
    perror("Не удалось создать временный файл");
    return NULL;
  }

  char Buf[GIF_STAMP_LEN + 1];
  GifFilePrivateType *Private;
  int status = 0;
  Gif = (GifFileType *)malloc(sizeof(GifFileType));

  if (Gif == NULL) {
    if (Error != NULL)
      *Error = D_GIF_ERR_NOT_ENOUGH_MEM;

    return NULL;
  }

  /*@i1@*/ memset(Gif, '\0', sizeof(GifFileType));

  Gif->SavedImages = NULL;
  Gif->SColorMap = NULL;

  Private = (GifFilePrivateType *)calloc(1, sizeof(GifFilePrivateType));

  if (Private == NULL) {
    if (Error != NULL)
      *Error = D_GIF_ERR_NOT_ENOUGH_MEM;
    free((char *)Gif);

    return NULL;
  }

  /*@i1@*/ memset(Private, '\0', sizeof(GifFilePrivateType));

  size_t bytesWritten = fwrite(image_buf, 1, size, tempFile);

  if (bytesWritten != size) {
    perror("Ошибка при записи данных в временный файл");
    fclose(tempFile);
    return NULL;
  } else {
    fprintf(stdout, "write: %zu\n", bytesWritten);
  }

  rewind(tempFile);

  Gif->Private = (void *)Private;
  Private->File = tempFile;
  Private->FileState = FILE_STATE_READ;
  Private->Read = NULL;
  Gif->UserData = NULL;

  if (READ(Gif, (unsigned char *)Buf, GIF_STAMP_LEN) != GIF_STAMP_LEN) {
    if (Error != NULL)
      *Error = D_GIF_ERR_READ_FAILED;
    free((char *)Private);
    free((char *)Gif);

    return NULL;
  }

  Buf[GIF_STAMP_LEN] = 0;

  if (strncmp(GIF_STAMP, Buf, GIF_VERSION_POS) != 0) {
    if (Error != NULL)
      *Error = D_GIF_ERR_NOT_GIF_FILE;
    free((char *)Private);
    free((char *)Gif);

    return NULL;
  }

  if (DGifGetScreenDesc(Gif) == GIF_ERROR) {
    free((char *)Private);
    free((char *)Gif);
    perror("error\n");

    return NULL;
  }

  Gif->Error = 0;

  Private->gif89 = (Buf[GIF_VERSION_POS + 1] == '9');

  return Gif;
}

//------------------------------------------------------------------------------

int GifToWebp(uint8_t *image_buf, size_t size, uint8_t **out_buf, int lossless) {
  int verbose = 0;
  int gif_error = GIF_ERROR;
  WebPMuxError err = WEBP_MUX_OK;
  int ok = 0;
  int out_size = 0;
  const W_CHAR *out_file = NULL;
  GifFileType *gif = NULL;
  int frame_duration = 0;
  int frame_timestamp = 0;
  GIFDisposeMethod orig_dispose = GIF_DISPOSE_NONE;

  WebPPicture frame;
  WebPPicture curr_canvas;
  WebPPicture prev_canvas;

  WebPAnimEncoder *enc = NULL;
  WebPAnimEncoderOptions enc_options;
  WebPConfig config;

  int frame_number = 0;
  int done;
  int c;
  int quiet = 0;
  WebPData webp_data;

  int keep_metadata = METADATA_XMP;
  WebPData icc_data;
  int stored_icc = 0;
  WebPData xmp_data;
  int stored_xmp = 0;
  int loop_count = 0;
  int stored_loop_count = 0;
  int loop_compatibility = 0;
  WebPMux *mux = NULL;

  int default_kmin = 1;
  int default_kmax = 1;

  if (!WebPConfigInit(&config) || !WebPAnimEncoderOptionsInit(&enc_options) ||
      !WebPPictureInit(&frame) || !WebPPictureInit(&curr_canvas) ||
      !WebPPictureInit(&prev_canvas)) {
    fprintf(stderr, "Error! Version mismatch!\n");
    FREE_WARGV_AND_RETURN(EXIT_FAILURE);
  }
  config.lossless = lossless;

  WebPDataInit(&webp_data);
  WebPDataInit(&icc_data);
  WebPDataInit(&xmp_data);

  if (default_kmin) {
    enc_options.kmin = config.lossless ? 9 : 3;
  }
  if (default_kmax) {
    enc_options.kmax = config.lossless ? 17 : 5;
  }

  if (!WebPValidateConfig(&config)) {
    fprintf(stderr, "Error! Invalid configuration.\n");
    goto End;
  }

  gif = GifFromUint8Array(image_buf, size, &gif_error);

  if (gif == NULL)
    goto End;

  done = 0;
  do {
    GifRecordType type;
    if (DGifGetRecordType(gif, &type) == GIF_ERROR)
      goto End;

    switch (type) {
    case IMAGE_DESC_RECORD_TYPE: {
      GIFFrameRect gif_rect;
      GifImageDesc *const image_desc = &gif->Image;

      if (!DGifGetImageDesc(gif))
        goto End;

      if (frame_number == 0) {
        if (verbose) {
          printf("Canvas screen: %d x %d\n", gif->SWidth, gif->SHeight);
        }
        if (gif->SWidth == 0 || gif->SHeight == 0) {
          image_desc->Left = 0;
          image_desc->Top = 0;
          gif->SWidth = image_desc->Width;
          gif->SHeight = image_desc->Height;
          if (gif->SWidth <= 0 || gif->SHeight <= 0) {
            goto End;
          }
          if (verbose) {
            printf("Fixed canvas screen dimension to: %d x %d\n", gif->SWidth,
                   gif->SHeight);
          }
        }
        frame.width = gif->SWidth;
        frame.height = gif->SHeight;
        frame.use_argb = 1;
        if (!WebPPictureAlloc(&frame))
          goto End;
        GIFClearPic(&frame, NULL);
        if (!(WebPPictureCopy(&frame, &curr_canvas) &&
              WebPPictureCopy(&frame, &prev_canvas))) {
          fprintf(stderr, "Error allocating canvas.\n");
          goto End;
        }

        GIFGetBackgroundColor(gif->SColorMap, gif->SBackGroundColor,
                              transparent_index,
                              &enc_options.anim_params.bgcolor);

        enc = WebPAnimEncoderNew(curr_canvas.width, curr_canvas.height,
                                 &enc_options);
        if (enc == NULL) {
          fprintf(stderr,
                  "Error! Could not create encoder object. Possibly due to "
                  "a memory error.\n");
          goto End;
        }
      }

      if (image_desc->Width == 0 || image_desc->Height == 0) {
        image_desc->Width = gif->SWidth;
        image_desc->Height = gif->SHeight;
      }

      if (!GIFReadFrame(gif, transparent_index, &gif_rect, &frame)) {
        goto End;
      }

      GIFBlendFrames(&frame, &gif_rect, &curr_canvas);

      if (!WebPAnimEncoderAdd(enc, &curr_canvas, frame_timestamp, &config)) {
        fprintf(stderr, "Error while adding frame #%d: %s\n", frame_number,
                WebPAnimEncoderGetError(enc));
        goto End;
      } else {
        ++frame_number;
      }

      GIFDisposeFrame(orig_dispose, &gif_rect, &prev_canvas, &curr_canvas);
      GIFCopyPixels(&curr_canvas, &prev_canvas);

      if (frame_duration <= 10) {
        frame_duration = 100;
      }

      frame_timestamp += frame_duration;

      orig_dispose = GIF_DISPOSE_NONE;
      frame_duration = 0;
      transparent_index = GIF_INDEX_INVALID;
      break;
    }
    case EXTENSION_RECORD_TYPE: {
      int extension;
      GifByteType *data = NULL;
      if (DGifGetExtension(gif, &extension, &data) == GIF_ERROR) {
        goto End;
      }

      if (data == NULL)
        continue;

      switch (extension) {
      case COMMENT_EXT_FUNC_CODE: {
        break;
      }
      case GRAPHICS_EXT_FUNC_CODE: {
        if (!GIFReadGraphicsExtension(data, &frame_duration, &orig_dispose,
                                      &transparent_index)) {
          goto End;
        }
        break;
      }
      case PLAINTEXT_EXT_FUNC_CODE: {
        break;
      }
      case APPLICATION_EXT_FUNC_CODE: {

        if (data[0] != 11)
          break;
        if (!memcmp(data + 1, "NETSCAPE2.0", 11) ||
            !memcmp(data + 1, "ANIMEXTS1.0", 11)) {
          if (!GIFReadLoopCount(gif, &data, &loop_count)) {
            goto End;
          }
          if (verbose) {
            fprintf(stderr, "Loop count: %d\n", loop_count);
          }
          stored_loop_count = loop_compatibility ? (loop_count != 0) : 1;
        } else {
          const int is_xmp = (keep_metadata & METADATA_XMP) && !stored_xmp &&
                             !memcmp(data + 1, "XMP DataXMP", 11);
          const int is_icc = (keep_metadata & METADATA_ICC) && !stored_icc &&
                             !memcmp(data + 1, "ICCRGBG1012", 11);
          if (is_xmp || is_icc) {
            if (!GIFReadMetadata(gif, &data, is_xmp ? &xmp_data : &icc_data)) {
              goto End;
            }
            if (is_icc) {
              stored_icc = 1;
            } else if (is_xmp) {
              stored_xmp = 1;
            }
          }
        }
        break;
      }
      default: {
        break;
      }
      }
      while (data != NULL) {
        if (DGifGetExtensionNext(gif, &data) == GIF_ERROR)
          goto End;
      }
      break;
    }
    case TERMINATE_RECORD_TYPE: {
      done = 1;
      break;
    }
    default: {
      if (verbose) {
        fprintf(stderr, "Skipping over unknown record type %d\n", type);
      }
      break;
    }
    }
  } while (!done);

  if (!WebPAnimEncoderAdd(enc, NULL, frame_timestamp, NULL)) {
    fprintf(stderr, "Error flushing WebP muxer.\n");
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
  }

  if (!WebPAnimEncoderAssemble(enc, &webp_data)) {
    fprintf(stderr, "%s\n", WebPAnimEncoderGetError(enc));
    goto End;
  }

  if (frame_number == 1) {
    loop_count = 0;
  } else if (!loop_compatibility) {
    if (!stored_loop_count) {
      if (frame_number > 1) {
        stored_loop_count = 1;
        loop_count = 1;
      }
    } else if (loop_count > 0 && loop_count < 65535) {
      loop_count += 1;
    }
  }

  if (loop_count == 0)
    stored_loop_count = 0;

  if (stored_loop_count || stored_icc || stored_xmp) {
    mux = WebPMuxCreate(&webp_data, 1);

    if (mux == NULL) {
      fprintf(stderr, "ERROR: Could not re-mux to add loop count/metadata.\n");
      goto End;
    }
    WebPDataClear(&webp_data);

    if (stored_loop_count) {
      WebPMuxAnimParams new_params;
      err = WebPMuxGetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK) {
        fprintf(stderr, "ERROR (%s): Could not fetch loop count.\n",
                ErrorString(err));
        goto End;
      }
      new_params.loop_count = loop_count;
      err = WebPMuxSetAnimationParams(mux, &new_params);
      if (err != WEBP_MUX_OK) {
        fprintf(stderr, "ERROR (%s): Could not update loop count.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_icc) {
      err = WebPMuxSetChunk(mux, "ICCP", &icc_data, 1);
      if (verbose) {
        fprintf(stderr, "ICC size: %d\n", (int)icc_data.size);
      }
      if (err != WEBP_MUX_OK) {
        fprintf(stderr, "ERROR (%s): Could not set ICC chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    if (stored_xmp) {
      err = WebPMuxSetChunk(mux, "XMP ", &xmp_data, 1);
      if (verbose) {
        fprintf(stderr, "XMP size: %d\n", (int)xmp_data.size);
      }
      if (err != WEBP_MUX_OK) {
        fprintf(stderr, "ERROR (%s): Could not set XMP chunk.\n",
                ErrorString(err));
        goto End;
      }
    }

    err = WebPMuxAssemble(mux, &webp_data);
    if (err != WEBP_MUX_OK) {
      fprintf(stderr,
              "ERROR (%s): Could not assemble when re-muxing to add "
              "loop count/metadata.\n",
              ErrorString(err));
      goto End;
    }
  }

  out_size = (int)webp_data.size;

  *out_buf = (uint8_t *)malloc(out_size);

  if (*out_buf == NULL) {
    fprintf(stderr, "ERROR: Could not allocate %d bytes for output.\n",
            out_size);
    goto End;
  }

  memcpy(*out_buf, webp_data.bytes, out_size);
  WebPDataClear(&webp_data);

  ok = 1;
  gif_error = GIF_OK;

End:
  WebPDataClear(&icc_data);
  WebPDataClear(&xmp_data);
  WebPMuxDelete(mux);
  WebPDataClear(&webp_data);
  WebPPictureFree(&frame);
  WebPPictureFree(&curr_canvas);
  WebPPictureFree(&prev_canvas);
  WebPAnimEncoderDelete(enc);

  if (gif_error != GIF_OK) {
    GIFDisplayError(gif, gif_error);
  }
  if (gif != NULL) {
#if LOCAL_GIF_PREREQ(5, 1)
    DGifCloseFile(gif, &gif_error);
#else
    DGifCloseFile(gif);
#endif
  }

  return out_size;
}

// Gif to WebP conversion
//------------------------------------------------------------------------------

struct Result result = {};

EMSCRIPTEN_KEEPALIVE
size_t get_mem_size(int width, int height) {
  return width * height * 4 * sizeof(uint8_t);
}

EMSCRIPTEN_KEEPALIVE
uint8_t *allocate_memory(size_t size) { return malloc(size); }

EMSCRIPTEN_KEEPALIVE
void deallocate_memory(uint8_t *p) { free(p); }

EMSCRIPTEN_KEEPALIVE
void encode(uint8_t *img_in, int width, int height, float quality) {
  uint8_t *img_out;
  size_t size;

  size = WebPEncodeRGBA(img_in, width, height, width * 4, quality, &img_out);

  result.buffer_pointer = img_out;
  result.buffer_size = size;
}

EMSCRIPTEN_KEEPALIVE
void encode_gif(uint8_t *img_in, int img_size, int lossless) {
  uint8_t *img_out = malloc(1024 * 1024 * 20);
  size_t size;

  size = GifToWebp(img_in, img_size, &img_out, lossless);

  result.buffer_pointer = img_out;
  result.buffer_size = size;
}

EMSCRIPTEN_KEEPALIVE
void free_result(uint8_t *result) { WebPFree(result); }

EMSCRIPTEN_KEEPALIVE
uint8_t *get_output_pointer() { return result.buffer_pointer; }

EMSCRIPTEN_KEEPALIVE
int get_output_size() { return result.buffer_size; }
