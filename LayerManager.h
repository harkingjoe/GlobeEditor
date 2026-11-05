// SPDX-FileCopyrightText: 2026 harkingjoe
// SPDX-License-Identifier: MIT
/*ピクセル配列の管理、レイヤーの合成、ファイル入出力*/

#ifndef __LAYERMANAGER_H__
#define __LAYERMANAGER_H__


#include <GL/freeglut.h> // GLuint を使うため

class LayerManager {
private:
    int img_w, img_h, img_ch_base;

    unsigned char* img_base;   // 背景画像
    unsigned char* img_middle; // 中間レイヤー
    unsigned char* img_layer;  // 編集レイヤー（描画対象）
    unsigned char* img_comp;   // 合成結果（画面に表示するテクスチャ用）

    GLuint tex_id;             // OpenGLのテクスチャID

public:
    LayerManager();
    ~LayerManager();

    // 初期化・画像読み込み
    bool loadBaseImage(const char* fname);
    bool loadLayer(const char* fname);
    bool loadMiddleLayer(const char* fname);

    void unloadMiddleLayer();

    // 合成処理
    void updateCompositeRect(int x, int y, int w, int h);
    void updateCompositeAll();

    // 保存・エクスポート
    bool saveLayer(const char* fname);
    bool exportComposite(const char* fname);

    // テクスチャをOpenGLに転送（描画の直前に呼ぶ）
    void updateTexture();

    // 各種ゲッター（他のクラスから情報を取得するための関数）
    int getWidth() const { return img_w; }
    int getHeight() const { return img_h; }
    GLuint getTextureID() const { return tex_id; }
    
    // 描画ツールなどが直接ピクセルをいじるためのポインタ取得
    unsigned char* getLayerBuffer() const { return img_layer; }
    unsigned char* getCompositeBuffer() const { return img_comp; }

private:
    void clearBuffers(); // 内部のメモリ解放用
};

#endif
