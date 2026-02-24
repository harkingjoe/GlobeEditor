#include "LayerManager.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdio>

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
        std::memcpy(img_layer, dat, w * h * 4);
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

    if (w == img_w && h == img_h) {
        if (img_middle) std::free(img_middle);
        img_middle = (unsigned char*)std::malloc(w * h * 4);

        // 上下反転してコピー
        int stride = w * 4;
        for (int y = 0; y < h; y++) {
            unsigned char* src_row = dat + (y * stride);
            unsigned char* dst_row = img_middle + ((h - 1 - y) * stride);
            std::memcpy(dst_row, src_row, stride);
        }

        updateCompositeAll();
        printf("Loaded middle layer (flipped): %s\n", fname);
        stbi_image_free(dat);
        return true;
    }
    
    printf("Size mismatch!\n");
    stbi_image_free(dat);
    return false;
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
