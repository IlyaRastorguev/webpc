#!/usr/bin/env bash

emcc -O3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' \
        -I libwebp \
        webp.c \
        libwebp/src/{dec,dsp,demux,enc,mux,utils}/*.c libwebp/sharpyuv/*.c
