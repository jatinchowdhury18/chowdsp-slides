# For some reason, this only works right on my Windows machine?
npx esbuild build-mathjax.js \
  --bundle \
  --format=iife \
  --platform=neutral \
  --target=es2020 \
  --minify \
  --outfile=mathjax-embedded.js
