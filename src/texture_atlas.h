#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize(size), mX(0), mY(0), mRowHeight(0) {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) { 
        // Check if the rectangle is wider than the bin.
        if (rectSize.x > mSize.x) {
            return {false, 0};
        }

        // Move to the next row if the rectangle does not fit horizontally.
        if (mX + rectSize.x > mSize.x) {
            mX = 0;
            mY += mRowHeight;
            mRowHeight = 0;
        }

        // Check if the rectangle fits vertically.
        if (mY + rectSize.y > mSize.y) {
            return {false, 0};
        }

        // Store the rectangle and assign its ID.
        size_t id = mRects.size();
        mRects.push_back(sf::FloatRect({mX, mY}, rectSize));

        // Update the position for the next rectangle.
        mX += rectSize.x;

        if (rectSize.y > mRowHeight) {
            mRowHeight = rectSize.y;
        }

        return {true, id};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { 
        return mRects[rectID];
    }

private:
    sf::Vector2f mSize;
    float mX;
    float mY;
    float mRowHeight;
    std::vector<sf::FloatRect> mRects;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, unsigned int pixels) { 
    sf::Vector2u size = img.getSize();
    sf::Vector2u newSize = {size.x + 2 * pixels, size.y + 2 * pixels};
    sf::Image padded(newSize);

    for (unsigned int y = 0; y < newSize.y; y++) {
        for (unsigned int x = 0; x < newSize.x; x++) {
            int srcX = (int)x - (int)pixels;
            int srcY = (int)y - (int)pixels;

            if (srcX < 0) srcX = 0;
            if (srcY < 0) srcY = 0;
            if (srcX >= (int)size.x) srcX = size.x - 1;
            if (srcY >= (int)size.y) srcY = size.y - 1;

            sf::Color color = img.getPixel({(unsigned int)srcX, (unsigned int)srcY});
            padded.setPixel({x, y}, color);
        }
    }
    return padded;
};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img;
        bool loaded = img.loadFromFile(filepath);
        assert(loaded);

        sf::Image padded = padImage(img, mPadding);
        sf::Vector2u paddedSize = padded.getSize();

        std::pair<bool, size_t> result =
            mPacker.pack(sf::Vector2f(paddedSize));
        assert(result.first);

        TextureID id = result.second;
        sf::FloatRect bounds = mPacker.getBounds(id);

        mTexture.update(padded, {
            (unsigned int)bounds.position.x,
            (unsigned int)bounds.position.y
        });

        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect bounds = mPacker.getBounds(id);
        sf::Vector2u atlasSize = mTexture.getSize();

        float x = bounds.position.x + mPadding;
        float y = bounds.position.y + mPadding;
        float width = bounds.size.x - 2 * mPadding;
        float height = bounds.size.y - 2 * mPadding;

        float u = x / atlasSize.x;
        float v = 1.0f - (y + height) / atlasSize.y;
        float uWidth = width / atlasSize.x;
        float vHeight = height / atlasSize.y;

        return sf::FloatRect({u, v}, {uWidth, vHeight});
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        sf::FloatRect bounds = mPacker.getBounds(id);
        unsigned int width = bounds.size.x - 2 * mPadding;
        unsigned int height = bounds.size.y - 2 * mPadding;

        return {width, height};
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
