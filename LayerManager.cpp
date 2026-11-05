#include "LayerManager.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


LayerManager::LayerManager() {
    img_base = nullptr;
    img_middle = nullptr;
    img_layer = nullptr;
    img_comp = nullptr;
    img_w = 0; img_h = 0; img_ch_base = 0;
    tex_id = 0;
}

LayerManager::~LayerManager() {
    clearBuffers();
}

void LayerManager::clearBuffers() {
    if (img_base) stbi_image_free(img_base);
    if (img_middle) std::free(img_middle);
    if (img_layer) std::free(img_layer);
    if (img_comp) std::free(img_comp);
    
    img_base = img_middle = img_layer = img_comp = nullptr;
}

bool LayerManager::loadBaseImage(const char* fname) {
    clearBuffers();

    img_base = stbi_load(fname, &img_w, &img_h, &img_ch_base, 0);
    if (!img_base) return false;

    size_t sz_layer = img_w * img_h * 4;
    img_layer = (unsigned char*)std::calloc(1, sz_layer); // ゼロクリアで確保(透明)
    img_comp = (unsigned char*)std::malloc(sz_layer);

    updateCompositeAll();

    // OpenGLテクスチャの生成
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // 初期テクスチャデータの転送
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_w, img_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_comp);

    return true;
}

bool LayerManager::loadLayer(const char* fname) {
    int w, h, c;
    unsigned char* dat = stbi_load(fname, &w, &h, &c, 4);
    if (!dat) return false;

    if (w == img_w && h == img_h) {
        int stride = w * 4;
        for (int y = 0; y < h; y++) {
            unsigned char* src_row = dat + (y * stride);                     // 元画像のy行目
            unsigned char* dst_row = img_layer + ((h - 1 - y) * stride); // 貼り付け先の下からy行目
            std::memcpy(dst_row, src_row, stride);
        }
        
        updateCompositeAll();
        printf("Loaded layer: %s\n", fname);
        stbi_image_free(dat);
        return true;
    }
    stbi_image_free(dat);
    return false;
}

bool LayerManager::loadMiddleLayer(const char* fname) {
    int w, h, c;
    unsigned char* dat = stbi_load(fname, &w, &h, &c, 4);
    if (!dat) {
        printf("Failed to load: %s\n", fname);
        return false;
    }

    if (img_w <= 0 || img_h <= 0) {
        printf("Base image is not loaded.\n");
        stbi_image_free(dat);
        return false;
    }

    // 縦横比の差が1%を超える画像は読み込まない
    const double src_ratio = static_cast<double>(w) / h;
    const double dst_ratio = static_cast<double>(img_w) / img_h;
    if (std::abs(src_ratio - dst_ratio) > dst_ratio * 0.01) {
        printf("Aspect ratio mismatch!\n");
        stbi_image_free(dat);
        return false;
    }

    const size_t dst_w = img_w;
    const size_t dst_h = img_h;
    unsigned char* resized = (unsigned char*)std::malloc(dst_w * dst_h * 4);
    if (!resized) {
        printf("Out of memory: %s\n", fname);
        stbi_image_free(dat);
        return false;
    }

    // 最近傍補間で背景サイズに変換し、上下反転して格納する
    // (合成時に半透明の画素を不透明として描くため、滑らかな補間は使わない)
    for (size_t y = 0; y < dst_h; y++) {
        const size_t src_y = y * h / dst_h;
        for (size_t x = 0; x < dst_w; x++) {
            const size_t src_x = x * w / dst_w;
            const unsigned char* src = dat + (src_y * w + src_x) * 4;
            unsigned char* dst = resized + ((dst_h - 1 - y) * dst_w + x) * 4;
            std::memcpy(dst, src, 4);
        }
    }

    if (img_middle) std::free(img_middle);
    img_middle = resized;

    updateCompositeAll();
    printf("Loaded middle layer (flipped): %s (%dx%d -> %dx%d)\n", fname, w, h, img_w, img_h);
    stbi_image_free(dat);
    return true;
}

void LayerManager::unloadMiddleLayer() {
    if (img_middle) {
        std::free(img_middle);
        img_middle = nullptr;
        updateCompositeAll(); // 中間レイヤーがない状態で再合成
        printf("Unloaded middle layer.\n");
    }
}

void LayerManager::updateCompositeRect(int x, int y, int w, int h) {
    if (!img_comp) return;
    int sx = std::max(0, x);
    int sy = std::max(0, y);
    int ex = std::min(img_w, x + w);
    int ey = std::min(img_h, y + h);

    for (int cy = sy; cy < ey; cy++) {
        for (int cx = sx; cx < ex; cx++) {
            int idx_base = (cy * img_w + cx) * img_ch_base;
            int idx_rgba = (cy * img_w + cx) * 4;

            unsigned char la = img_layer[idx_rgba + 3];

            if (la > 0) {
                // 1. 最上位: 編集レイヤー
                img_comp[idx_rgba]   = img_layer[idx_rgba];
                img_comp[idx_rgba+1] = img_layer[idx_rgba+1];
                img_comp[idx_rgba+2] = img_layer[idx_rgba+2];
                img_comp[idx_rgba+3] = 255;
            } 
            else if (img_middle && img_middle[idx_rgba + 3] > 0) {
                // 2. 中間レイヤー
                img_comp[idx_rgba]   = img_middle[idx_rgba];
                img_comp[idx_rgba+1] = img_middle[idx_rgba+1];
                img_comp[idx_rgba+2] = img_middle[idx_rgba+2];
                img_comp[idx_rgba+3] = 255;
            }
            else {
                // 3. 最下位: 背景画像
                img_comp[idx_rgba]   = img_base[idx_base];
                img_comp[idx_rgba+1] = img_base[idx_base+1];
                img_comp[idx_rgba+2] = img_base[idx_base+2];
                img_comp[idx_rgba+3] = 255;
            }
        }
    }
}

void LayerManager::updateCompositeAll() {
    updateCompositeRect(0, 0, img_w, img_h);
}

bool LayerManager::saveLayer(const char* fname) {
    if (!img_layer) return false;
    int stride = img_w * 4;
    unsigned char *last_row = img_layer + (img_h - 1) * stride;
    return stbi_write_png(fname, img_w, img_h, 4, last_row, -stride) != 0;
}

bool LayerManager::exportComposite(const char* fname) {
    if (!img_comp) return false;
    int stride = img_w * 4;
    unsigned char *last_row = img_comp + (img_h - 1) * stride;
    return stbi_write_png(fname, img_w, img_h, 4, last_row, -stride) != 0;
}

void LayerManager::updateTexture() {
    if (tex_id > 0 && img_comp) {
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img_w, img_h, GL_RGBA, GL_UNSIGNED_BYTE, img_comp);
    }
}
