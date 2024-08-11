#!/usr/bin/env bash

emcc -g -O3 -s WASM=1 -D WEBP_HAVE_GIF=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS='["cwrap"]' \
        -I libwebp \
        webp.c \
        libwebp/src/{dec,dsp,demux,enc,mux,utils}/*.c libwebp/examples/gifdec.c libwebp/sharpyuv/*.c giflib/lib/*.c
