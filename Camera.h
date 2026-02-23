/*視点の管理、画面座標と3D空間の変換*/

#ifndef __CAMERA_H__
#define __CAMERA_H__


#include "MathUtils.h"   // Vec3 などの構造体を使用するため
#include <GL/freeglut.h> // GLdouble や OpenGL の関数を使用するため

class Camera {
private:
    float cam_dist;        // カメラとターゲットの距離
    float spin_ang;        // 地球儀の自転角度
    GLdouble base_mat[16]; // トラックボール回転用の累積行列
    
    int win_w;             // ウィンドウの幅
    int win_h;             // ウィンドウの高さ
    bool is_resetting;     // リセットアニメーション中かどうか

public:
    Camera(int w = 800, int h = 600);

    // ウィンドウサイズが変更されたとき (reshape時) に呼ぶ
    void updateWindowSize(int w, int h);
    
    // 描画時 (display関数等) にカメラの変換行列を適用する
    void applyTransform();

    // マウスドラッグによる視点回転 (Shiftなしのトラックボール回転など)
    void rotateView(int dx, int dy, float rot_sens);

    void spinGlobe(int last_x, int last_y, int x, int y, float rot_sens);

    // スクリーン座標 (mx, my) から3D空間へのレイ (org, dir) を計算
    bool getRay(int mx, int my, Vec3* org, Vec3* dir);

    // 3D座標 (p) をスクリーン座標 (sx, sy) に変換
    bool projectToScreen(Vec3 p, float* sx, float* sy);

    // リセットアニメーションの更新 (idle関数で呼ぶ)
    void updateResetAnimation();

    // アクセサ（外部から状態を変更・取得するため）
    void startReset() { is_resetting = true; }
    void stopReset()  { is_resetting = false; }
    bool isResetting() const { return is_resetting; }
    
    float getCamDist() const { return cam_dist; }
    void addCamDist(float delta);
    
    void addSpin(float delta) { spin_ang += delta; }
};

#endif
