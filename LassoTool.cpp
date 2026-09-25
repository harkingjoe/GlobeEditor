#include "LassoTool.h"
#include "LayerManager.h"
#include "Camera.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------
// コンストラクタ・初期化
// ---------------------------------------------------------
LassoTool::LassoTool() {
    is_active = false;
    is_selecting = false;
    has_floating = false;
}

LassoTool::~LassoTool() {
    points.clear();
    floats.clear();
    lasso_path_3d.clear();
}

// ---------------------------------------------------------
// モード切替と変換ロジック
// ---------------------------------------------------------
void LassoTool::enterMode(LayerManager* layerMgr) {
    if (is_active) return;
    
    points.clear(); floats.clear(); 
    has_floating = false; is_selecting = false; lasso_path_3d.clear();

    int img_w = layerMgr->getWidth();
    int img_h = layerMgr->getHeight();
    unsigned char* img_layer = layerMgr->getLayerBuffer();

    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w; x++) {
            int idx = (y * img_w + x) * 4;
            if (img_layer[idx+3] > 0) {
                float u = (float)x / img_w;
                float v = (float)y / img_h;
                
                Vec3 p3 = uv2xyz(u + 0.5f/img_w, v + 0.5f/img_h, SPH_RAD);
                
                CloudPoint cp;
                cp.p = p3;
                cp.r = img_layer[idx];
                cp.g = img_layer[idx+1];
                cp.b = img_layer[idx+2];
                points.push_back(cp);
            }
        }
    }
    std::memset(img_layer, 0, img_w * img_h * 4);
    layerMgr->updateCompositeAll();
    is_active = true;
}

void LassoTool::exitMode(LayerManager* layerMgr) {
    if (!is_active) return;

    if (has_floating) {
        pasteSelection();
    }

    rasterizePoints(layerMgr);
    
    points.clear(); floats.clear(); lasso_path_3d.clear();
    is_selecting = false;
    is_active = false;
}

void LassoTool::rasterizePoints(LayerManager* layerMgr) {
    int img_w = layerMgr->getWidth();
    int img_h = layerMgr->getHeight();
    unsigned char* img_layer = layerMgr->getLayerBuffer();

    std::memset(img_layer, 0, img_w * img_h * 4);

    for (auto &cp : points) {
        float u, v;
        xyz2uv(cp.p, SPH_RAD, &u, &v); 
        
        int cx = (int)(u * img_w);
        int cy = (int)(v * img_h);

        int r = 1; 
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int tx = (cx + dx + img_w) % img_w; 
                int ty = cy + dy;
                if (ty >= 0 && ty < img_h) {
                    int idx = (ty * img_w + tx) * 4;
                    img_layer[idx]   = cp.r;
                    img_layer[idx+1] = cp.g;
                    img_layer[idx+2] = cp.b;
                    img_layer[idx+3] = 255;
                }
            }
        }
    }
    layerMgr->updateCompositeAll();
}

// ---------------------------------------------------------
// 操作系ロジック
// ---------------------------------------------------------
void LassoTool::startSelection(int mx, int my, Camera* camera) {
    if (!is_active) return;
    lasso_path_3d.clear();
    is_selecting = true;
    recordSelection(mx, my, camera);
}

void LassoTool::recordSelection(int mx, int my, Camera* camera) {
    if (!is_selecting) return;

    // スクリーン座標を球面上の3D座標に変換して保持する
    // (画面座標のままだと、視点を動かした時に枠線が地球上でずれてしまうため)
    Vec3 org, dir, hit;
    if (!camera->getRay(mx, my, &org, &dir)) return;
    if (!hit_sph(org, dir, SPH_RAD, &hit)) {
        // 球の外側をなぞった場合は、レイが球に最も近づく点を球面へ投影する (≒地平線上の点)
        float t = -dot(org, dir);
        Vec3 near_p = {org.x + dir.x * t, org.y + dir.y * t, org.z + dir.z * t};
        Vec3 n = normalize(near_p);
        hit = (Vec3){n.x * SPH_RAD, n.y * SPH_RAD, n.z * SPH_RAD};
    }
    lasso_path_3d.push_back(hit);
}

void LassoTool::cutSelection() {
    is_selecting = false;
    if (!is_active || lasso_path_3d.size() < 3) {
        lasso_path_3d.clear(); return;
    }

    // 枠線の中心方向を求め、その点で球に接する平面へ心射投影(gnomonic)して内外判定する。
    // 心射投影では大円が直線になるため、球面上の多角形をそのまま2Dの多角形として扱える。
    Vec3 sum = {0,0,0};
    for (const auto& p3 : lasso_path_3d) sum = add(sum, normalize(p3));
    if (dot(sum, sum) < 1e-8f) { lasso_path_3d.clear(); return; }
    Vec3 c = normalize(sum);

    // 接平面の基底 (e1, e2)
    Vec3 ref = (std::fabs(c.x) < 0.9f) ? (Vec3){1,0,0} : (Vec3){0,1,0};
    float rc = dot(ref, c);
    Vec3 e1 = normalize(sub(ref, (Vec3){c.x * rc, c.y * rc, c.z * rc}));
    Vec3 e2 = {c.y * e1.z - c.z * e1.y, c.z * e1.x - c.x * e1.z, c.x * e1.y - c.y * e1.x};

    const float min_cos = 1e-3f; // 中心から90度以上離れた点は投影できないので除外
    std::vector<UVPoint> poly;
    for (const auto& p3 : lasso_path_3d) {
        Vec3 d = normalize(p3);
        float dc = dot(d, c);
        if (dc > min_cos) poly.push_back({dot(d, e1) / dc, dot(d, e2) / dc});
    }
    if (poly.size() < 3) { lasso_path_3d.clear(); return; }

    auto inPoly = [&](float x, float y) {
        bool in = false;
        for (size_t i = 0, j = poly.size()-1; i < poly.size(); j = i++) {
            if (((poly[i].v > y) != (poly[j].v > y)) &&
                (x < (poly[j].u - poly[i].u) * (y - poly[i].v) / (poly[j].v - poly[i].v) + poly[i].u))
                in = !in;
        }
        return in;
    };

    floats.clear();
    auto it = std::remove_if(points.begin(), points.end(), [&](CloudPoint &cp){
        Vec3 d = normalize(cp.p);
        float dc = dot(d, c);
        if (dc <= min_cos) return false;
        if (inPoly(dot(d, e1) / dc, dot(d, e2) / dc)) {
            floats.push_back(cp);
            return true;
        }
        return false;
    });
    points.erase(it, points.end());

    if (!floats.empty()) {
        has_floating = true;
    } else {
        lasso_path_3d.clear();
    }
}

void LassoTool::scaleSelection(float s, Camera* camera) {
    if (floats.empty()) return;

    Vec3 sum = {0,0,0};
    for (const auto& cp : floats) sum = add(sum, cp.p);
    Vec3 cen_dir = normalize(sum);
    Vec3 cen = {cen_dir.x * SPH_RAD, cen_dir.y * SPH_RAD, cen_dir.z * SPH_RAD};

    for (auto& cp : floats) {
        Vec3 diff = sub(cp.p, cen);
        Vec3 s_diff = {diff.x * s, diff.y * s, diff.z * s};
        Vec3 n_pos = add(cen, s_diff);
        cp.p = normalize(n_pos);
        cp.p.x *= SPH_RAD; cp.p.y *= SPH_RAD; cp.p.z *= SPH_RAD;
    }

    for (auto& p3 : lasso_path_3d) {
         Vec3 diff = sub(p3, cen);
         Vec3 s_diff = {diff.x * s, diff.y * s, diff.z * s};
         Vec3 n_pos = add(cen, s_diff);
         p3 = normalize(n_pos);
         p3.x *= SPH_RAD; p3.y *= SPH_RAD; p3.z *= SPH_RAD;
    }
}

void LassoTool::moveSelection(int dx, int dy, Camera* camera) {
    if (!has_floating) return;

    // カメラの行列から現在の視点の向き（Up, Right）を抽出
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    camera->applyTransform();
    GLdouble mv[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glPopMatrix();

    Vec3 cam_right = normalize((Vec3){ (float)mv[0], (float)mv[4], (float)mv[8] });
    Vec3 cam_up    = normalize((Vec3){ (float)mv[1], (float)mv[5], (float)mv[9] });

    float sensitivity = 0.002f;
    float ang_h = dx * sensitivity; 
    float ang_v = dy * sensitivity; 

    for(auto &cp : floats) {
        cp.p = rotate_point(cp.p, cam_up, ang_h);
        cp.p = rotate_point(cp.p, cam_right, ang_v);
    }

    for(auto &p3 : lasso_path_3d) {
        p3 = rotate_point(p3, cam_up, ang_h);
        p3 = rotate_point(p3, cam_right, ang_v);
    }
}

void LassoTool::rotateSelection(int last_x, int last_y, int x, int y, Camera* camera) {
    if (!has_floating || floats.empty()) return;

    // 1. 点群の重心（3D空間上の回転軸）を求める
    Vec3 sum = {0,0,0};
    for (const auto& cp : floats) sum = add(sum, cp.p);
    Vec3 cen_dir = normalize(sum);
    Vec3 cen = {cen_dir.x * SPH_RAD, cen_dir.y * SPH_RAD, cen_dir.z * SPH_RAD};

    // 2. 重心をスクリーン座標に投影し、画面上での回転中心(cx, cy)を求める
    float cx, cy;
    if (!camera->projectToScreen(cen, &cx, &cy)) return;

    // 3. 回転中心から見た、前回と今回のマウスの角度を計算
    float a1 = std::atan2(last_y - cy, last_x - cx);
    float a2 = std::atan2(y - cy, x - cx);
    float diff = a2 - a1;

    // 角度の境界(-180度 ⇔ 180度)を跨いだ時のラップアラウンド補正
    if (diff > M_PI) diff -= 2.0f * M_PI;
    if (diff < -M_PI) diff += 2.0f * M_PI;

    // 画面のY軸は下向きなので、diffが正＝画面上で時計回り。
    // 一方、3D空間の手前向き法線を軸とする回転は「反時計回り」が正になるため符号を反転する。
    float ang = -diff; 

    // 4. 回転を適用
    for(auto &cp : floats) {
        cp.p = rotate_point(cp.p, cen_dir, ang);
    }
    for(auto &p3 : lasso_path_3d) {
        p3 = rotate_point(p3, cen_dir, ang);
    }
}

void LassoTool::pasteSelection() {
    if (!has_floating) return;
    
    for(auto &cp : floats) points.push_back(cp);
    floats.clear();
    
    has_floating = false;
    lasso_path_3d.clear();
}

void LassoTool::importFloatingPoints(const std::vector<CloudPoint>& new_floats) {
    if (!is_active) return;
    if (has_floating) pasteSelection();
    
    floats = new_floats;
    has_floating = true;
}

// ---------------------------------------------------------
// 描画ロジック
// ---------------------------------------------------------
void LassoTool::draw() const {
    if (!is_active) return;

    // --- 点群の描画 ---
    glDisable(GL_TEXTURE_2D);
    glPointSize(2.0f);

    glBegin(GL_POINTS);
    // 1. 通常の点群
    for(auto &cp : points) {
        glColor3ub(cp.r, cp.g, cp.b);
        glVertex3f(cp.p.x, cp.p.y, cp.p.z);
    }
    // 2. 浮遊している点群（赤くハイライト）
    if(has_floating) {
        glColor3f(1.0f, 0.2f, 0.2f);
        for(auto &cp : floats) {
            glVertex3f(cp.p.x, cp.p.y, cp.p.z);
        }
    }
    glEnd();

    // --- 枠線の描画 ---
    if (!lasso_path_3d.empty()) {
        drawLassoLine();
    }
}

void LassoTool::drawLassoLine() const {
    // 枠線は球面上の3D線として描画する (地球の裏側に回った部分は深度テストで隠れる)
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);

    glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0xF0F0);
    glLineWidth(2.0f); glColor3f(0.0f, 1.0f, 1.0f);

    const float line_rad = SPH_RAD * 1.003f; // 点群に埋もれないよう少し浮かせる
    const float max_step = 0.02f;            // 大円に沿って分割する角度の目安 [rad]

    glBegin(GL_LINE_LOOP);
    size_t n = lasso_path_3d.size();
    for (size_t i = 0; i < n; i++) {
        Vec3 a = normalize(lasso_path_3d[i]);
        Vec3 b = normalize(lasso_path_3d[(i + 1) % n]);
        float cs = dot(a, b);
        if (cs > 1.0f) cs = 1.0f;
        if (cs < -1.0f) cs = -1.0f;
        int steps = (int)std::ceil(std::acos(cs) / max_step);
        if (steps < 1) steps = 1;
        // 始点～終点の手前までを出力 (終点は次の区間の始点になる)
        for (int k = 0; k < steps; k++) {
            float t = (float)k / steps;
            Vec3 p = normalize((Vec3){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t});
            glVertex3f(p.x * line_rad, p.y * line_rad, p.z * line_rad);
        }
    }
    glEnd();

    glDisable(GL_LINE_STIPPLE);
}
