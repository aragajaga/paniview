#!/bin/sh

baseDir="$PWD"

svgDir="$baseDir/assets/svg"
pngDir="$baseDir/assets/png"
bmpDir="$baseDir/assets/bmp"
icoDir="$baseDir/assets/ico"
resDir="$baseDir/res"

mkdir -p "$bmpDir"
mkdir -p "$pngDir"
mkdir -p "$icoDir"

gen_icon() {
  convert -background none -density 48  "$svgDir/$1.svg" "$pngDir/$1@16px.png"
  convert -background none -density 96  "$svgDir/$1.svg" "$pngDir/$1@32px.png"
  convert -background none -density 144 "$svgDir/$1.svg" "$pngDir/$1@48px.png"
  convert -background none -density 768 "$svgDir/$1.svg" "$pngDir/$1@256px.png"
   
  convert -background none \
    "$pngDir/$1@16px.png" \
    "$pngDir/$1@32px.png" \
    "$pngDir/$1@48px.png" \
    "$pngDir/$1@256px.png" \
    "$icoDir/$1.ico"

  cp "$icoDir/$1.ico" "$resDir/"
}

gen_strip() {
  convert -background none -density 96  "$svgDir/$1.svg" "$pngDir/$1@16px.png"
  convert -background none -density 144 "$svgDir/$1.svg" "$pngDir/$1@24px.png"
  convert -background none -density 192 "$svgDir/$1.svg" "$pngDir/$1@32px.png"

  convert -background none "$pngDir/$1@16px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1@16px.bmp"
  convert -background none "$pngDir/$1@24px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1@24px.bmp"
  convert -background none "$pngDir/$1@32px.png" -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1@32px.bmp"
  cp "$bmpDir/$1@16px.bmp" "$resDir/"
  cp "$bmpDir/$1@24px.bmp" "$resDir/"
  cp "$bmpDir/$1@32px.bmp" "$resDir/"

  convert -background none "$pngDir/$1@16px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_hot@16px.bmp"
  convert -background none "$pngDir/$1@24px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_hot@24px.bmp"
  convert -background none "$pngDir/$1@32px.png" -modulate 100,130,100 -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_hot@32px.bmp"
  cp "$bmpDir/$1_hot@16px.bmp" "$resDir/"
  cp "$bmpDir/$1_hot@24px.bmp" "$resDir/"
  cp "$bmpDir/$1_hot@32px.bmp" "$resDir/"

  convert -background none "$pngDir/$1@16px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_disabled@16px.bmp"
  convert -background none "$pngDir/$1@24px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_disabled@24px.bmp"
  convert -background none "$pngDir/$1@32px.png" -colorspace Gray -define bmp:format=bmp3 -define bmp3:alpha=true "$bmpDir/$1_disabled@32px.bmp"
  cp "$bmpDir/$1_disabled@16px.bmp" "$resDir/"
  cp "$bmpDir/$1_disabled@24px.bmp" "$resDir/"
  cp "$bmpDir/$1_disabled@32px.bmp" "$resDir/"
}

gen_icon "ApplicationIcon"
gen_icon "ApplicationIconDebug"
gen_strip "toolbar"
