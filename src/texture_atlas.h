#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
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
    RectPacker(const sf::Vector2f& size) : mSize{size} {}

    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        constexpr size_t kInvalid = std::numeric_limits<size_t>::max();

        // can never fit, even on an empty line
        if (rectSize.x > mSize.x || rectSize.y > mSize.y) {
            return {false, kInvalid};
        }

        // tentative placement; only committed if the rectangle fits
        float lineY = mLineY;
        float x = mX;
        float lineMaxH = mLineMaxHeight;

        if (x + rectSize.x > mSize.x) {  // doesn't fit horizontally: start a new line
            lineY += lineMaxH;
            x = 0.f;
            lineMaxH = 0.f;
        }
        if (lineY + rectSize.y > mSize.y) {  // doesn't fit vertically
            return {false, kInvalid};
        }

        mRects.push_back(sf::FloatRect({x, lineY}, rectSize));
        mLineY = lineY;
        mX = x + rectSize.x;
        mLineMaxHeight = std::max(lineMaxH, rectSize.y);
        return {true, mRects.size() - 1};
    }


    sf::FloatRect getBounds(size_t rectID) const {
        assert(rectID < mRects.size());
        return mRects[rectID];
    }

private:
    sf::Vector2f mSize;
    float mLineY = 0.f;         
    float mX = 0.f;             
    float mLineMaxHeight = 0.f; 
    std::vector<sf::FloatRect> mRects;
};

inline sf::Image padImage(const sf::Image& img, uint pixels) {
    const sf::Vector2u size = img.getSize();
    sf::Image out(sf::Vector2u(size.x + 2 * pixels, size.y + 2 * pixels), sf::Color::Transparent);
    if (size.x == 0 || size.y == 0) {
        return out;
    }

    const int w = static_cast<int>(size.x);
    const int h = static_cast<int>(size.y);
    const int p = static_cast<int>(pixels);

    for (unsigned y = 0; y < out.getSize().y; ++y) {
        const int sy = std::clamp(static_cast<int>(y) - p, 0, h - 1);
        for (unsigned x = 0; x < out.getSize().x; ++x) {
            const int sx = std::clamp(static_cast<int>(x) - p, 0, w - 1);
            out.setPixel({x, y}, img.getPixel({static_cast<unsigned>(sx), static_cast<unsigned>(sy)}));
        }
    }
    return out;
}

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img;
        if (!img.loadFromFile(filepath)) {
            throw std::runtime_error("Failed to load image: " + filepath);
        }
        const sf::Image padded = padImage(img, static_cast<uint>(mPadding));

        const auto [ok, id] = mPacker.pack(sf::Vector2f(padded.getSize()));
        if (!ok) {
            throw std::runtime_error("Texture atlas is full, cannot pack: " + filepath);
        }

        const sf::FloatRect bounds = mPacker.getBounds(id);
        mTexture.update(padded, sf::Vector2u(bounds.position));
        return id;  
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        const sf::FloatRect b = mPacker.getBounds(id);
        const float pad = static_cast<float>(mPadding);
        const sf::Vector2f atlas(mTexture.getSize());

        const sf::Vector2f pos = b.position + sf::Vector2f(pad, pad);  // top-left, pixels
        const sf::Vector2f size = b.size - sf::Vector2f(2 * pad, 2 * pad);

        const float u = pos.x / atlas.x;
        const float v = 1.f - (pos.y + size.y) / atlas.y;
        return sf::FloatRect({u, v}, {size.x / atlas.x, size.y / atlas.y});
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        const sf::FloatRect b = mPacker.getBounds(id);
        const float pad = static_cast<float>(mPadding);
        return sf::Vector2u(b.size - sf::Vector2f(2 * pad, 2 * pad));
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H