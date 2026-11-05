// SPDX-FileCopyrightText: 2026 harkingjoe
// SPDX-License-Identifier: MIT
#include "Camera.h"
#include <cmath>

Camera::Camera(int w, int h) : win_w(w), win_h(h) {
    cam_dist = 5.0f;
    spin_ang = 0.0f;
    is_resetting = false;
    
    // 単位行列で初期化
    for(int i = 0; i < 16; i++) {
        base_mat[i] = (i % 5 == 0) ? 1.0 : 0.0;
    }
}

void Camera::updateWindowSize(int w, int h) {
    win_w = w;
    win_h = h > 0 ? h : 1; // ゼロ除算防止
}

void Camera::applyTransform() {
    glLoadIdentity();
    gluLookAt(0.0, 0.0, cam_dist,  0.0, 0.0, 0.0,  0.0, 1.0, 0.0);
    glMultMatrixd(base_mat);
    glRotatef(-23.4f, 0.0f, 0.0f, 1.0f);
    glRotatef(spin_ang, 0.0f, 1.0f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
}

void Camera::rotateView(int dx, int dy, float rot_sens) {
    float sens = (45.0f / (float)win_h) * rot_sens; 

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRotatef((float)dx * sens, 0.0f, 1.0f, 0.0f); 
    glRotatef((float)dy * sens, 1.0f, 0.0f, 0.0f); 
    glMultMatrixd(base_mat);                       
    glGetDoublev(GL_MODELVIEW_MATRIX, base_mat);   
    glPopMatrix();
}

void Camera::spinGlobe(int last_x, int last_y, int x, int y, float rot_sens) {
    Vec3 org1, dir1, hit1;
    bool valid1 = getRay(last_x, last_y, &org1, &dir1);
    if (valid1) {
        if (!hit_sph(org1, dir1, SPH_RAD, &hit1)) {
            float t = -dot(org1, dir1);
            hit1 = (Vec3){org1.x + dir1.x * t, org1.y + dir1.y * t, org1.z + dir1.z * t};
        }
    }

    Vec3 org2, dir2, hit2;
    bool valid2 = getRay(x, y, &org2, &dir2);
    if (valid2) {
        if (!hit_sph(org2, dir2, SPH_RAD, &hit2)) {
            float t = -dot(org2, dir2);
            hit2 = (Vec3){org2.x + dir2.x * t, org2.y + dir2.y * t, org2.z + dir2.z * t};
        }
    }

    if (valid1 && valid2) {
        float a1 = std::atan2(hit1.y, hit1.x);
        float a2 = std::atan2(hit2.y, hit2.x);
        
        float diff = (a2 - a1) * 180.0f / (float)M_PI;
        
        if (diff > 180.0f)  diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;
        
        float r1 = std::sqrt(hit1.x * hit1.x + hit1.y * hit1.y);
        float weight = std::sqrt(r1 / SPH_RAD); 
        
        spin_ang += diff * weight * rot_sens;
    }
}

bool Camera::getRay(int mx, int my, Vec3* org, Vec3* dir) {
    GLint vp[4]; 
    GLdouble mv[16], pj[16], x, y, z;
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    applyTransform(); // クラス内の共通関数で視点をセット

    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pj);
    glGetIntegerv(GL_VIEWPORT, vp);
    glPopMatrix();
    
    float win_y = (float)vp[3] - (float)my;
    
    // 近平面上の点（始点）
    if (!gluUnProject(mx, win_y, 0.0, mv, pj, vp, &x, &y, &z)) return false;
    *org = (Vec3){(float)x, (float)y, (float)z};
    
    // 遠平面上の点
    gluUnProject(mx, win_y, 1.0, mv, pj, vp, &x, &y, &z);
    Vec3 dst = (Vec3){(float)x, (float)y, (float)z};
    
    // 方向ベクトルを計算して正規化 (MathUtilsの関数を使用)
    *dir = normalize(sub(dst, *org));

    return true;
}

bool Camera::projectToScreen(Vec3 p, float* sx, float* sy) {
    GLint vp[4];
    GLdouble mv[16], pj[16], x, y, z;
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    applyTransform();

    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, pj);
    glGetIntegerv(GL_VIEWPORT, vp);
    glPopMatrix();

    if(!gluProject(p.x, p.y, p.z, mv, pj, vp, &x, &y, &z)) return false;

    *sx = (float)x;
    *sy = (float)(vp[3] - y);

    return (z < 1.0);
}

void Camera::updateResetAnimation() {
    if (!is_resetting) return;

    float sp = 0.1f; float eps = 0.01f;
        
    spin_ang += (0.0f - spin_ang) * sp;
    cam_dist += (5.0f - cam_dist) * sp;
        
    double tr = base_mat[0] + base_mat[5] + base_mat[10];
    double cos_val = (tr - 1.0) / 2.0;
    if (cos_val > 1.0) cos_val = 1.0;
    if (cos_val < -1.0) cos_val = -1.0;
        
    double angle = std::acos(cos_val); 
        
    if (angle > eps) {
        double ax = base_mat[6] - base_mat[9];
        double ay = base_mat[8] - base_mat[2];
        double az = base_mat[1] - base_mat[4];
            
        angle += (0.0 - angle) * sp;
            
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glRotatef(angle * 180.0 / M_PI, ax, ay, az);
        glGetDoublev(GL_MODELVIEW_MATRIX, base_mat);
        glPopMatrix();
    } else {
        for(int i = 0; i < 16; i++) base_mat[i] = (i % 5 == 0) ? 1.0 : 0.0;
        angle = 0.0;
    }

    if (std::abs(spin_ang) < eps && std::abs(cam_dist - 5.0f) < eps && angle == 0.0) {
        is_resetting = false;
    }
}

void Camera::addCamDist(float delta) {
    cam_dist += delta;
    if (cam_dist < 2.5f) cam_dist = 2.5f;
    if (cam_dist > 15.0f) cam_dist = 15.0f;
}
