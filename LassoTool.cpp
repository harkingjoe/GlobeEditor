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
    lasso_path.clear();
    lasso_path_3d.clear();
}

// ---------------------------------------------------------
// モード切替と変換ロジック
// ---------------------------------------------------------
void LassoTool::enterMode(LayerManager* layerMgr) {
    if (is_active) return;
    
    points.clear(); floats.clear(); 
    has_floating = false; lasso_path.clear();

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
    
    points.clear(); floats.clear(); lasso_path.clear();
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
void LassoTool::startSelection(int mx, int my) {
    if (!is_active) return;
    lasso_path.clear();
    is_selecting = true;
    recordSelection(mx, my);
}

void LassoTool::recordSelection(int mx, int my) {
    lasso_path.push_back({(float)mx, (float)my});
}

bool LassoTool::isInPolyScreen(float x, float y) const {
    bool c = false;
    for (size_t i = 0, j = lasso_path.size()-1; i < lasso_path.size(); j = i++) {
        if (((lasso_path[i].v > y) != (lasso_path[j].v > y)) &&
            (x < (lasso_path[j].u - lasso_path[i].u) * (y - lasso_path[i].v) / (lasso_path[j].v - lasso_path[i].v) + lasso_path[i].u))
            c = !c;
    }
    return c;
}

void LassoTool::cutSelection(Camera* camera) {
    if (!is_active || lasso_path.size() < 3) {
        lasso_path.clear(); return;
    }

    lasso_path_3d.clear();
    for (auto &uv : lasso_path) {
        Vec3 org, dir, hit;
        if (camera->getRay((int)uv.u, (int)uv.v, &org, &dir)) {
            if (hit_sph(org, dir, SPH_RAD, &hit)) {
                lasso_path_3d.push_back(hit);
            }
        }
    }

    floats.clear();
    auto it = std::remove_if(points.begin(), points.end(), [&](CloudPoint &cp){
        float sx, sy;
        if(camera->projectToScreen(cp.p, &sx, &sy)) {
            if(isInPolyScreen(sx, sy)) {
                floats.push_back(cp);
                return true; 
            }
        }
        return false;
    });
    points.erase(it, points.end());

    if (!floats.empty()) {
        has_floating = true;
    } else {
        lasso_path.clear();
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

    lasso_path.clear();
    for(auto &p3 : lasso_path_3d) {
        float sx, sy;
        if (camera->projectToScreen(p3, &sx, &sy)) lasso_path.push_back({sx, sy});
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

    lasso_path.clear();
    for(auto &p3 : lasso_path_3d) {
        float sx, sy;
        if (camera->projectToScreen(p3, &sx, &sy)) {
            lasso_path.push_back({sx, sy});
        }
    }
}

void LassoTool::pasteSelection() {
    if (!has_floating) return;
    
    for(auto &cp : floats) points.push_back(cp);
    floats.clear();
    
    has_floating = false;
    lasso_path.clear();
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
void LassoTool::draw(Camera* camera, int win_w, int win_h) {
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
    if (!lasso_path.empty()) {
        drawLassoLine(win_w, win_h);
    }
}

void LassoTool::drawLassoLine(int win_w, int win_h) const {
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
    
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, win_w, win_h, 0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    
    glEnable(GL_LINE_STIPPLE); glLineStipple(1, 0xF0F0);
    glLineWidth(2.0f); glColor3f(0.0f, 1.0f, 1.0f);

    glBegin(GL_LINE_LOOP);
    for(auto &p : lasso_path) glVertex2f(p.u, p.v);
    glEnd();

    glDisable(GL_LINE_STIPPLE); glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
}
