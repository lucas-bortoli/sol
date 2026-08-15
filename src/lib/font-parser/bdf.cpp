#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "raylib.h"

// Helper to safely parse hex to unsigned int
unsigned int HexToUInt(const std::string& hexStr) {
    try {
        return std::stoul(hexStr, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

Font LoadFontBDF(const char* fileName) {
    Font font = {0};
    std::ifstream file(fileName);
    if (!file.is_open()) {
        TraceLog(LOG_ERROR, "BDF: Failed to open file %s", fileName);
        return font;
    }

    std::string line;
    int baseSize = 0;
    int fontAscent = 0;
    int fontDescent = 0;
    int globalBbxH = 0;
    int globalBbxYOff = 0;

    std::vector<GlyphInfo> glyphs;
    GlyphInfo currentGlyph = {0};
    int currentBBX[4] = {0, 0, 0, 0};  // w, h, xoff, yoff
    bool inBitmap = false;
    int bitmapRow = 0;

    while (std::getline(file, line)) {
        // Strip trailing carriage return if CRLF
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "FONT_ASCENT") {
            iss >> fontAscent;
        } else if (token == "FONT_DESCENT") {
            iss >> fontDescent;
        } else if (token == "PIXEL_SIZE") {
            iss >> baseSize;
        } else if (token == "FONTBOUNDINGBOX") {
            int gw;
            iss >> gw >> globalBbxH >> globalBbxYOff;  // w, h, xoff, yoff
        } else if (token == "ENCODING") {
            iss >> currentGlyph.value;
        } else if (token == "DWIDTH") {
            iss >> currentGlyph.advanceX;
        } else if (token == "BBX") {
            iss >> currentBBX[0] >> currentBBX[1] >> currentBBX[2] >>
                currentBBX[3];
            currentGlyph.offsetX = currentBBX[2];
        } else if (token == "BITMAP") {
            inBitmap = true;
            bitmapRow = 0;

            // Setup Raylib Image for this glyph
            currentGlyph.image.width = currentBBX[0];
            currentGlyph.image.height = currentBBX[1];
            currentGlyph.image.mipmaps = 1;
            currentGlyph.image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;

            int dataSize = currentBBX[0] * currentBBX[1];
            if (dataSize > 0) {
                // Must use MemAlloc so Raylib's UnloadFont() can clean it up
                // safely
                currentGlyph.image.data = MemAlloc(dataSize);
                std::memset(currentGlyph.image.data, 0, dataSize);
            } else {
                currentGlyph.image.data = nullptr;
            }
        } else if (token == "ENDCHAR") {
            inBitmap = false;

            // Calculate Raylib's offsetY (distance from top of the line down to
            // the glyph's top) BDF bounding box Y offset is from the baseline
            // to the bottom of the glyph. Glyph Top = BDF Y-Offset + BDF Height
            int glyphTop = currentBBX[3] + currentBBX[1];
            currentGlyph.offsetY = fontAscent - glyphTop;

            // Skip unencoded characters (-1)
            if (currentGlyph.value >= 0) {
                glyphs.push_back(currentGlyph);
            } else {
                if (currentGlyph.image.data) {
                    MemFree(currentGlyph.image.data);
                    currentGlyph.image.data = nullptr;
                }
            }

            // Reset for next glyph
            currentGlyph = {0};
        } else if (inBitmap) {
            // Write hex data into Image data array
            if (currentGlyph.image.data && bitmapRow < currentBBX[1]) {
                int widthBytes = (currentBBX[0] + 7) / 8;
                unsigned char* imgData =
                    (unsigned char*)currentGlyph.image.data;

                for (int i = 0; i < widthBytes; i++) {
                    if (i * 2 >= (int)line.length()) break;
                    std::string hexByte = line.substr(i * 2, 2);
                    unsigned int byteVal = HexToUInt(hexByte);

                    // Expand bits to byte pixels
                    for (int bit = 0; bit < 8; bit++) {
                        int pxX = i * 8 + bit;
                        if (pxX < currentBBX[0]) {
                            // BDF hex bits read left to right (MSB to LSB)
                            bool isSet = (byteVal & (1 << (7 - bit))) != 0;
                            imgData[bitmapRow * currentBBX[0] + pxX] =
                                isSet ? 255 : 0;
                        }
                    }
                }
                bitmapRow++;
            }
        }
    }

    if (glyphs.empty()) {
        TraceLog(LOG_ERROR, "BDF: No glyphs loaded from %s", fileName);
        return font;
    }

    // Resolve global font metrics if missing
    if (fontAscent == 0 && fontDescent == 0) {
        fontAscent = globalBbxH + globalBbxYOff;
        fontDescent = -globalBbxYOff;
    }
    if (baseSize == 0) {
        baseSize = fontAscent + fontDescent;
    }

    // Populate Font structure
    font.baseSize = baseSize;
    font.glyphCount = (int)glyphs.size();
    font.glyphPadding = 1;  // 1px padding in the atlas prevents bleeding

    // Copy glyphs to heap array
    font.glyphs = (GlyphInfo*)MemAlloc(font.glyphCount * sizeof(GlyphInfo));
    for (int i = 0; i < font.glyphCount; i++) {
        font.glyphs[i] = glyphs[i];
    }

    // Let Raylib pack the individual GlyphInfo images into a single Texture
    // Atlas
    Rectangle* recs = nullptr;
    Image atlas = GenImageFontAtlas(
        font.glyphs, &recs, font.glyphCount, font.baseSize, font.glyphPadding, 0
    );

    font.texture = LoadTextureFromImage(atlas);
    font.recs = recs;

    // Clean up temporary atlas image
    UnloadImage(atlas);

    TraceLog(
        LOG_INFO,
        "BDF: Loaded font %s with %d glyphs",
        fileName,
        font.glyphCount
    );

    return font;
}