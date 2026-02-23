#include "BrushTool.h"
#include "LayerManager.h"
#include "Camera.h"
#include "HistoryManager.h"
#include "UIManager.h"
#include <cmath>
#include <cstring>
#include <vector>

BrushTool::BrushTool() {
    last_u = -1.0f;
    last_v = -1.0f;
}

BrushTool::~BrushTool() {}

void BrushTool::resetStroke() {
    last_u = -1.0f;
    last_v = -1.0f;
}

void BrushTool::applyBrush(float u, float v, LayerManager* layerMgr, UIManager* uiMgr, bool is_erase) {
    int img_w = layerMgr->getWidth();
    int img_h = layerMgr->getHeight();
    unsigned char* img_layer = layerMgr->getLayerBuffer();
    
    int ix = (int)(u * img_w) % img_w;
    int iy = (int)(v * img_h) % img_h;
    if (ix < 0) ix += img_w;
    if (iy < 0) iy += img_h;
    
    float sz = is_erase ? uiMgr->getEraseBrushSize() : uiMgr->getDrawBrushSize();
    int rad_i = (int)sz; 
    
    const unsigned char* col = uiMgr->getColor();
    int min_x = img_w, min_y = img_h, max_x = 0, max_y = 0;

    for (int y = -rad_i; y <= rad_i; y++) {
        for (int x = -rad_i; x <= rad_i; x++) {
            if (x*x + y*y > rad_i*rad_i) continue; // 円形にするための判定
            
            int dx = (ix + x + img_w) % img_w;
            int dy = iy + y;
            if (dy < 0 || dy >= img_h) continue;
            
            int idx = (dy * img_w + dx) * 4;
            
            if (is_erase) {
                img_layer[idx+3] = 0; 
            } else {
                img_layer[idx]   = col[0];
                img_layer[idx+1] = col[1];
                img_layer[idx+2] = col[2];
                img_layer[idx+3] = 255;
            }
            if (dx < min_x) { min_x = dx; }
            if (dx > max_x) { max_x = dx; }
            if (dy < min_y) { min_y = dy; }
            if (dy > max_y) { max_y = dy; }
        }
    }
    
    // 更新領域をLayerManagerに通知
    if (max_x < min_x) {
        layerMgr->updateCompositeAll(); 
    } else {
        layerMgr->updateCompositeRect(min_x, min_y, (max_x - min_x + 1), (max_y - min_y + 1));
    }
}

void BrushTool::paint(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr, bool is_erase) {
    Vec3 org, dir, hit;
    float u, v;
    
    if (!camera->getRay(mx, my, &org, &dir)) { last_u = -1; return; }
    if (!hit_sph(org, dir, SPH_RAD, &hit)) { last_u = -1; return; }
    xyz2uv(hit, SPH_RAD, &u, &v);
    
    if (last_u < 0 || std::fabs(u - last_u) > 0.5f) {
        applyBrush(u, v, layerMgr, uiMgr, is_erase);
    } else {
        // マウスが早く動いた時のための線形補間（隙間埋め）
        float dst = std::sqrt(std::pow(u - last_u, 2) + std::pow(v - last_v, 2));
        int img_w = layerMgr->getWidth();
        int img_h = layerMgr->getHeight();
        float px_dst = dst * (img_w > img_h ? img_w : img_h);
        
        int stps = (int)(px_dst * 1.5f);
        if (stps < 1) stps = 1;
        
        for (int i = 0; i <= stps; i++) {
            float t = (float)i / stps;
            applyBrush(last_u * (1.0f - t) + u * t, last_v * (1.0f - t) + v * t, layerMgr, uiMgr, is_erase);
        }
    }
    last_u = u; last_v = v;
}

void BrushTool::pickColor(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr) {
    Vec3 org, dir, hit;
    if (!camera->getRay(mx, my, &org, &dir)) return;
    if (!hit_sph(org, dir, SPH_RAD, &hit)) return;
    
    float u, v; 
    xyz2uv(hit, SPH_RAD, &u, &v);
    
    int img_w = layerMgr->getWidth();
    int img_h = layerMgr->getHeight();
    unsigned char* img_comp = layerMgr->getCompositeBuffer();
    
    int ix = (int)(u * img_w) % img_w;
    int iy = (int)(v * img_h) % img_h;
    if (ix < 0) ix += img_w; 
    if (iy < 0) iy += img_h;
    
    int idx = (iy * img_w + ix) * 4;
    // 取得した色をUIマネージャーにセットする
    uiMgr->setColor(img_comp[idx], img_comp[idx+1], img_comp[idx+2]);
}

void BrushTool::floodFill(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr, HistoryManager* histMgr, bool is_erase) {
    Vec3 org, dir, hit;
    if (!camera->getRay(mx, my, &org, &dir)) return;
    if (!hit_sph(org, dir, SPH_RAD, &hit)) return;
    
    float u, v;
    xyz2uv(hit, SPH_RAD, &u, &v);
    
    int img_w = layerMgr->getWidth();
    int img_h = layerMgr->getHeight();
    unsigned char* img_layer = layerMgr->getLayerBuffer();
    
    int sx = (int)(u * img_w) % img_w;
    int sy = (int)(v * img_h) % img_h;
    if (sx < 0) sx += img_w; 
    if (sy < 0) sy += img_h;

    int s_idx = (sy * img_w + sx) * 4;
    unsigned char target_col[4];
    std::memcpy(target_col, &img_layer[s_idx], 4);

    unsigned char fill_col[4];
    if (is_erase) {
        fill_col[0]=0; fill_col[1]=0; fill_col[2]=0; fill_col[3]=0;
    } else {
        const unsigned char* col = uiMgr->getColor();
        fill_col[0]=col[0]; fill_col[1]=col[1]; fill_col[2]=col[2]; fill_col[3]=255;
    }

    if (std::memcmp(target_col, fill_col, 4) == 0) return;

    // 塗りつぶし実行前に履歴を保存
    histMgr->push_und(img_layer); 

    std::vector<bool> visited(img_w * img_h, false);
    std::vector<Point> q;
    q.push_back({sx, sy});
    visited[sy * img_w + sx] = true;
    
    int dx_arr[] = {1, -1, 0, 0};
    int dy_arr[] = {0, 0, 1, -1};

    while (!q.empty()) {
        Point p = q.back(); q.pop_back();
        int idx = (p.y * img_w + p.x) * 4;
        std::memcpy(&img_layer[idx], fill_col, 4);

        for (int i = 0; i < 4; i++) {
            int nx = p.x + dx_arr[i];
            int ny = p.y + dy_arr[i];
            
            if (ny < 0 || ny >= img_h) continue;
            if (nx < 0) nx = img_w - 1;
            if (nx >= img_w) nx = 0;
            
            int n_idx_pix = ny * img_w + nx;
            if (visited[n_idx_pix]) continue;
            
            int n_idx = n_idx_pix * 4;
            if (std::memcmp(&img_layer[n_idx], target_col, 4) == 0) {
                visited[n_idx_pix] = true;
                q.push_back({nx, ny});
            }
        }
    }
    layerMgr->updateCompositeAll();
}
