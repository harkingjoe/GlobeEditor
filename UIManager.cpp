#include "UIManager.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- UI配置設定（このファイル内だけで有効） ---
namespace {
    const float BR_MAX = 20.0f;
    const int UI_BX = 10; const int UI_BY = 10;
    const int UI_W  = 210; const int UI_H  = 40; const int UI_GAP = 10;
    const int UI_CRC_X = 52; const int UI_NUM_X = 30; 
    const int UI_RGB_MARGIN_B = 150; 
    const int MAN_BW = 60; const int MAN_BH = 30; const int MAN_MG = 10;
    const int F_BTN_SZ = 30; const int F_BTN_GP = 10;
    
    int clamp_val(int val, int min_v, int max_v) {
        if (val < min_v) return min_v;
        if (val > max_v) return max_v;
        return val;
    }
}

UIManager::UIManager() {
    current_tool = TGT_BRUSH;
    active_target = TGT_NONE;
    input_str = "";

    br_sz_drw = 3.0f;
    br_sz_ers = 10.0f;
    COL[0] = 0; COL[1] = 0; COL[2] = 0;

    font_idx = 1;
    sens_idx = 1;
    rot_sens = 1.0f;

    show_menu = false;
    menu_scroll_y = 0.0f;
    max_scroll_y = 0.0f;

    save_msg_timer = 0;
    is_saving = false;
    is_exporting = false;
    last_msg_type = 0;
}

UIManager::~UIManager() {}

// ---------------------------------------------------------
// メッセージ制御
// ---------------------------------------------------------
void UIManager::startSaveMessage() {
    if (is_saving || is_exporting) return;
    is_saving = true;      
    save_msg_timer = 300; 
    last_msg_type = 0;
}

void UIManager::startExportMessage() {
    if (is_saving || is_exporting) return;
    is_exporting = true;
    save_msg_timer = 300;
    last_msg_type = 0;
}

void UIManager::finishMessage(int type) {
    if (type == 1) { is_saving = false; last_msg_type = 1; }
    else if (type == 2) { is_exporting = false; last_msg_type = 2; }
}

void UIManager::updateMessageTimer() {
    if (save_msg_timer > 0) save_msg_timer--;
}

// ---------------------------------------------------------
// 入力処理（戻り値 true ならイベント消費済み）
// ---------------------------------------------------------
int UIManager::handleMouse(int btn, int state, int x, int y, int win_w, int win_h, bool is_lasso_mode) {
    if (state == GLUT_DOWN && active_target != TGT_NONE) {
        active_target = TGT_NONE; input_str = "";
    }

    if (btn == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Menu Helpボタン
        float bx = win_w - MAN_BW - MAN_MG;
        if (x >= bx && x <= bx + MAN_BW && y >= MAN_MG && y <= MAN_MG + MAN_BH) {
            show_menu = !show_menu; 
            if (show_menu) menu_scroll_y = 0.0f; 
            return 1;
        }

        if (show_menu) {
            // Font buttons
            float by = 60; float bx_st = win_w - 40 - 200;
            for (int i = 0; i < 3; i++) {
                float b = bx_st + i * (F_BTN_SZ + F_BTN_GP);
                if (x >= b && x <= b + F_BTN_SZ && y >= by && y <= by + F_BTN_SZ) { 
                    font_idx = i; return 1; 
                }
            }
            // Sensitivity buttons
            float sens_y = 100;
            for (int i = 0; i < 3; i++) {
                float b = bx_st + i * (F_BTN_SZ + F_BTN_GP);
                if (x >= b && x <= b + F_BTN_SZ && y >= sens_y && y <= sens_y + F_BTN_SZ) { 
                    sens_idx = i;
                    if (i == 0) rot_sens = 0.5f;     
                    else if (i == 1) rot_sens = 1.0f;
                    else rot_sens = 2.0f;             
                    return 1; 
                }
            }
            return 1; // メニュー開閉時は他のクリックを無視
        }

        // Exportボタン判定
        float ex_w = 80; float ex_h = 30; float ex_m = 20;
        float ex_x = win_w - ex_w - ex_m;
        float ex_y = win_h - ex_h - ex_m;
        if (x >= ex_x && x <= ex_x + ex_w && y >= ex_y && y <= ex_y + ex_h) {
            return 2; // 2 = Exportボタンが押された
        }

        // タブ切り替え
        UITarget top_tgts[] = {TGT_BRUSH, TGT_ERASER, TGT_LASSO, TGT_FILL, TGT_PICKER};
        for (int i = 0; i < 5; i++) { 
            int ty = UI_BY + i * (UI_H + UI_GAP);
            if (x >= UI_BX && x <= UI_BX + UI_W && y >= ty && y <= ty + UI_H) {
                current_tool = top_tgts[i];
                // ブラシと消しゴム以外は、数値入力をアクティブにしない
                if (current_tool == TGT_BRUSH || current_tool == TGT_ERASER) {
                    active_target = top_tgts[i];
                } else {
                    active_target = TGT_NONE;
                }
                input_str = "";
                return 1; // タブがクリックされた
            }
        }
        
        // RGB変更
        UITarget rgb_tgts[] = {TGT_R, TGT_G, TGT_B};
        int base_y = win_h - UI_RGB_MARGIN_B;
        for (int i = 0; i < 3; i++) {
            int ry = base_y + i * (UI_H + UI_GAP);
            if (x >= UI_BX && x <= UI_BX + UI_W && y >= ry && y <= ry + UI_H) {
                active_target = rgb_tgts[i]; input_str = ""; 
                return 1;
            }
        }
        
        // UI領域をクリックしたかどうか
        bool on_ui = (x < UI_BX + UI_W + 20) && ((y < UI_BY + 250) || (y > win_h - UI_RGB_MARGIN_B));
        if (on_ui) return 1;
    }
    return 0;
}

bool UIManager::handleWheel(int dir, int x, int y, int win_h, bool is_lasso_mode) {
    if (show_menu) {
        float speed = 30.0f;
        if (dir > 0) menu_scroll_y -= speed; else menu_scroll_y += speed;
        if (menu_scroll_y < 0) menu_scroll_y = 0.0f;
        if (menu_scroll_y > max_scroll_y) menu_scroll_y = max_scroll_y;
        return true;
    }

    if (!is_lasso_mode) {
        // ブラシサイズ
        for (int i = 0; i < 2; i++) {
            int ty = UI_BY + i * (UI_H + UI_GAP);
            if (x >= UI_BX && x <= UI_BX + UI_W && y >= ty && y <= ty + UI_H) {
                float diff = (dir > 0) ? -1.0f : 1.0f;
                if (i == 0) br_sz_drw = clamp_val((int)br_sz_drw + (int)diff, 1, (int)BR_MAX);
                else br_sz_ers = clamp_val((int)br_sz_ers + (int)diff, 1, (int)BR_MAX);
                if (active_target == (i==0 ? TGT_BRUSH : TGT_ERASER)) { input_str = ""; active_target = TGT_NONE; }
                return true;
            }
        }
    }

    // カラー
    int base_y = win_h - UI_RGB_MARGIN_B;
    for (int i = 0; i < 3; i++) {
        int ry = base_y + i * (UI_H + UI_GAP);
        if (x >= UI_BX && x <= UI_BX + UI_W && y >= ry && y <= ry + UI_H) {
            int change = (dir > 0) ? 5 : -5;
            COL[i] = (unsigned char)clamp_val((int)COL[i] + change, 0, 255);
            if (active_target == (i==0?TGT_R:(i==1?TGT_G:TGT_B))) { input_str = ""; active_target = TGT_NONE; }
            return true; 
        }
    }
    return false;
}

bool UIManager::handleKeyboard(unsigned char key) {
    if (active_target != TGT_NONE) {
        if (isdigit(key)) { if (input_str.length() < 3) input_str += key; }
        else if (key == 13) { // Enter
            if (input_str.length() > 0) {
                int val = std::atoi(input_str.c_str());
                if (active_target == TGT_BRUSH) br_sz_drw = clamp_val(val, 1, (int)BR_MAX);
                else if (active_target == TGT_ERASER) br_sz_ers = clamp_val(val, 1, (int)BR_MAX);
                else if (active_target == TGT_R) COL[0] = clamp_val(val, 0, 255);
                else if (active_target == TGT_G) COL[1] = clamp_val(val, 0, 255);
                else if (active_target == TGT_B) COL[2] = clamp_val(val, 0, 255);
            }
            active_target = TGT_NONE; input_str = "";
        } 
        else if (key == 8) { if (!input_str.empty()) input_str.pop_back(); } // Backspace
        else if (key == 27) { active_target = TGT_NONE; input_str = ""; } // ESC
        return true;
    }
    return false;
}

// ---------------------------------------------------------
// 描画ヘルパー群
// ---------------------------------------------------------
void* UIManager::getFont() {
    switch (font_idx) {
        case 0: return GLUT_BITMAP_HELVETICA_12;
        case 1: return GLUT_BITMAP_HELVETICA_18;
        case 2: return GLUT_BITMAP_TIMES_ROMAN_24;
        default: return GLUT_BITMAP_HELVETICA_18;
    }
}

int UIManager::getLineStep() {
    switch (font_idx) {
        case 0: return 24;
        case 1: return 32;
        case 2: return 42;
        default: return 32;
    }
}

void UIManager::drawStr(float x, float y, const char* str) {
    glRasterPos2f(x, y);
    void* font = getFont();
    for (const char* c = str; *c != '\0'; c++) glutBitmapCharacter(font, *c);
}

void UIManager::drawCirc(float x, float y, float rad) {
    glBegin(GL_TRIANGLE_FAN); glVertex2f(x, y);
    for (int i = 0; i <= 24; i++) {
        float tht = 2.0f * M_PI * (float)i / 24.0f;
        glVertex2f(x + rad * std::cos(tht), y + rad * std::sin(tht));
    }
    glEnd();
}

void UIManager::drawCircCol(float x, float y, float rad, const unsigned char* col) {
    glColor3ub(col[0], col[1], col[2]);
    drawCirc(x, y, rad);
}

void UIManager::drawStrOutline(float x, float y, const char* str, float alpha) {
    void* font = GLUT_BITMAP_HELVETICA_18; 
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    float offsets[8][2] = {{-1,-1}, {0,-1}, {1,-1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    for (int i = 0; i < 8; i++) {
        glRasterPos2f(x + offsets[i][0], y + offsets[i][1]);
        for (const char* c = str; *c != '\0'; c++) glutBitmapCharacter(font, *c);
    }
    glColor4f(0.2f, 1.0f, 0.2f, alpha); 
    glRasterPos2f(x, y);
    for (const char* c = str; *c != '\0'; c++) glutBitmapCharacter(font, *c);
}

// ---------------------------------------------------------
// メイン描画処理
// ---------------------------------------------------------
void UIManager::draw(int win_w, int win_h, bool is_lasso_mode, bool has_floating) {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, win_w, win_h, 0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_TEXTURE_2D);

    // Help Button
    float bx = win_w - MAN_BW - MAN_MG; float by = MAN_MG;
    if (show_menu) glColor3f(0.8f, 0.4f, 0.4f); else glColor3f(0.8f, 0.8f, 0.8f);
    glRectf(bx, by, bx+MAN_BW, by+MAN_BH);
    glColor3f(0,0,0); glBegin(GL_LINE_LOOP); glVertex2f(bx,by); glVertex2f(bx+MAN_BW,by);
    glVertex2f(bx+MAN_BW,by+MAN_BH); glVertex2f(bx,by+MAN_BH); glEnd();
    drawStr(bx+10, by+20, "Help");

    // Lasso Status
    if (is_lasso_mode) {
        glColor3f(1,0,0);
        if (has_floating) drawStr(win_w/2-50, 30, "[Lasso: Moving]");
        else drawStr(win_w/2-50, 30, "[Lasso: Select]");
    }

    // Tabs
    char buf[64];
    const char* top_lbls[] = {"Brush Size", "Eraser Size", "Lasso Tool", "Flood Fill", "Color Picker"};
    UITarget top_tgts[] = {TGT_BRUSH, TGT_ERASER, TGT_LASSO, TGT_FILL, TGT_PICKER};
    
    for (int i = 0; i < 5; i++) {
        int ty = UI_BY + i * (UI_H + UI_GAP);
        bool isActive = (current_tool == top_tgts[i]);
        if (isActive) glColor3f(1,1,1); else glColor3f(0.8f,0.8f,0.8f);
        
        glRectf(UI_BX, ty, UI_BX+UI_W, ty+UI_H);
        
        if (isActive) { glLineWidth(2); glColor3f(0.2f,0.2f,0.8f); }
        else { glLineWidth(1); glColor3f(0,0,0); }
        
        glBegin(GL_LINE_LOOP); glVertex2f(UI_BX,ty); glVertex2f(UI_BX+UI_W,ty);
        glVertex2f(UI_BX+UI_W,ty+UI_H); glVertex2f(UI_BX,ty+UI_H); glEnd();
        glLineWidth(1); glColor3f(0,0,0);
        
        drawStr(UI_BX+10, ty+25, top_lbls[i]);
        
        if (i < 2) {
            float val = (i == 0) ? br_sz_drw : br_sz_ers;
            if (active_target == top_tgts[i]) {
                std::sprintf(buf, "%s_", input_str.c_str()); drawStr(UI_BX+UI_W-60, ty+25, buf);
            } else {
                drawCirc(UI_BX+UI_W-UI_CRC_X, ty+UI_H/2, val);
                std::sprintf(buf, "%.0f", val); drawStr(UI_BX+UI_W-UI_NUM_X, ty+25, buf);
            }
        } else {
            // ★修正: Lasso, Fill, Picker の場合は (Active) を表示
            if (isActive) drawStr(UI_BX+UI_W-80, ty+25, "(Active)");
        }
    }

    // RGB
    const char* rgb_lbls[3] = {"R", "G", "B"};
    float rgb_colors[3][3] = {{1,0.8f,0.8f}, {0.8f,1,0.8f}, {0.8f,0.8f,1}};
    UITarget rgb_tgts[] = {TGT_R, TGT_G, TGT_B};
    int base_y = win_h - UI_RGB_MARGIN_B;
    for (int i = 0; i < 3; i++) {
        int ry = base_y + i * (UI_H + UI_GAP);
        if (active_target == rgb_tgts[i]) glColor3f(1,1,1);
        else glColor3fv(rgb_colors[i]);
        glRectf(UI_BX, ry, UI_BX+UI_W, ry+UI_H);
        if (active_target == rgb_tgts[i]) { glLineWidth(2); glColor3f(0.2f,0.2f,0.8f); }
        else { glLineWidth(1); glColor3f(0,0,0); }
        glBegin(GL_LINE_LOOP); glVertex2f(UI_BX,ry); glVertex2f(UI_BX+UI_W,ry);
        glVertex2f(UI_BX+UI_W,ry+UI_H); glVertex2f(UI_BX,ry+UI_H); glEnd();
        glLineWidth(1); glColor3f(0,0,0);
        if (active_target == rgb_tgts[i]) std::sprintf(buf, "%s: %s_", rgb_lbls[i], input_str.c_str());
        else std::sprintf(buf, "%s: %d", rgb_lbls[i], COL[i]);
        drawStr(UI_BX+10, ry+20, buf);
    }
    
    // Preview
    int preview_y = base_y + 3 * (UI_H + UI_GAP) + 10;
    glColor3f(1,1,1); drawStr(UI_BX+10, preview_y+15, "Current:");
    drawCircCol(UI_BX+100, preview_y+10, 12, COL);

    if (show_menu) drawMenu(win_w, win_h);

    // Export Button
    float ex_w = 80; float ex_h = 30; float ex_m = 20;
    float ex_x = win_w - ex_w - ex_m;
    float ex_y = win_h - ex_h - ex_m;
    glColor3f(0.3f, 0.3f, 0.3f);
    glRectf(ex_x, ex_y, ex_x + ex_w, ex_y + ex_h);
    glColor3f(1, 1, 1); glLineWidth(1);
    glBegin(GL_LINE_LOOP); 
    glVertex2f(ex_x, ex_y); glVertex2f(ex_x+ex_w, ex_y);
    glVertex2f(ex_x+ex_w, ex_y+ex_h); glVertex2f(ex_x, ex_y+ex_h); 
    glEnd();
    drawStr(ex_x + 18, ex_y + 20, "Export"); 

    // Messages
    if (save_msg_timer > 0) {
        float alpha = 1.0f;
        if (!is_saving && !is_exporting && save_msg_timer < 120) { 
            alpha = (float)save_msg_timer / 120.0f;
        }

        const char* msg_text = "";
        float r = 1.0f, g = 1.0f, b = 1.0f; 

        if (is_saving) { msg_text = "Saving..."; r = 1.0f; g = 1.0f; b = 0.2f; }
        else if (is_exporting) { msg_text = "Exporting..."; r = 0.2f; g = 1.0f; b = 1.0f; }
        else {
            r = 0.2f; g = 1.0f; b = 0.2f; 
            if (last_msg_type == 1) msg_text = "Project Saved";
            else if (last_msg_type == 2) msg_text = "Export Completed";
        }

        if (msg_text[0] != '\0') {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float mx = win_w / 2.0f;
            float my = 80.0f; 

            void* font_used = GLUT_BITMAP_HELVETICA_18; 
            int text_w = 0;
            for (const char* c = msg_text; *c != '\0'; c++) {
                text_w += glutBitmapWidth(font_used, *c);
            }

            float padding = 40.0f;
            float bw = (float)text_w + padding; 
            
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f * alpha);
            glRectf(mx - bw/2, my - 20, mx + bw/2, my + 10);
            
            glColor4f(r, g, b, alpha);
            drawStrOutline(mx - (float)text_w / 2.0f, my, msg_text, alpha);
            
            glColor3f(1, 1, 1);
            glDisable(GL_BLEND);
        }
    }

    glEnable(GL_TEXTURE_2D); glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void UIManager::drawMenu(int win_w, int win_h) {
    glColor4f(0,0,0,0.9f); glRectf(40, 40, win_w-40, win_h-40);
    glColor3f(1,1,1); glLineWidth(2);
    glBegin(GL_LINE_LOOP);
    glVertex2f(40,40); glVertex2f(win_w-40,40);
    glVertex2f(win_w-40,win_h-40); glVertex2f(40,win_h-40); glEnd();

    float btn_y = 60; float btn_x_st = win_w - 40 - 200;
    drawStr(btn_x_st-100, btn_y+20, "Text Size:");
    const char* lbls[3] = {"S", "M", "L"};
    for (int i = 0; i < 3; i++) {
        float bx = btn_x_st + i*(F_BTN_SZ+F_BTN_GP);
        if (i == font_idx) glColor3f(0.6f,0.6f,0.9f);
        else glColor3f(0.4f,0.4f,0.4f);
        glRectf(bx, btn_y, bx+F_BTN_SZ, btn_y+F_BTN_SZ);
        glColor3f(1,1,1); glBegin(GL_LINE_LOOP); glVertex2f(bx,btn_y); glVertex2f(bx+F_BTN_SZ,btn_y);
        glVertex2f(bx+F_BTN_SZ,btn_y+F_BTN_SZ); glVertex2f(bx,btn_y+F_BTN_SZ); glEnd();
        glRasterPos2f(bx+10, btn_y+20);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, lbls[i][0]);
    }

    float sens_y = 100;
    drawStr(btn_x_st-110, sens_y+20, "Rot. Speed:");
    const char* sens_lbls[3] = {"L", "M", "H"};
    for (int i = 0; i < 3; i++) {
        float bx = btn_x_st + i*(F_BTN_SZ+F_BTN_GP);
        if (i == sens_idx) glColor3f(0.9f, 0.6f, 0.6f);
        else glColor3f(0.4f, 0.4f, 0.4f);
        glRectf(bx, sens_y, bx+F_BTN_SZ, sens_y+F_BTN_SZ);
        glColor3f(1,1,1); glBegin(GL_LINE_LOOP); glVertex2f(bx,sens_y); glVertex2f(bx+F_BTN_SZ,sens_y);
        glVertex2f(bx+F_BTN_SZ,sens_y+F_BTN_SZ); glVertex2f(bx,sens_y+F_BTN_SZ); glEnd();
        glRasterPos2f(bx+10, sens_y+20);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, sens_lbls[i][0]);
    }
    
    int top_margin = 140; int bottom_margin = 40;
    glEnable(GL_SCISSOR_TEST);
    int sc_h = std::max(0, win_h - top_margin - bottom_margin);
    glScissor(40, bottom_margin, win_w-80, sc_h);
    
    
    float start_ty = 180.0f;
    float tx = 70; float ty = start_ty - menu_scroll_y;
    float step = (float)getLineStep();
    float half_step = step * 0.5f;
    
    // --- メニューテキスト描画 ---
    glColor3f(1.0f, 1.0f, 1.0f); 
    drawStr(tx, ty, "=== Operation Manual ==="); ty += step + half_step;
    
    // 1. 視点操作
    glColor3f(0.7f, 0.8f, 1.0f); drawStr(tx, ty, "[ View & Camera ]"); ty += step;
    glColor3f(1,1,1);
    drawStr(tx, ty, "  Mid-Drag : Rotate Globe"); ty += step;
    drawStr(tx, ty, "  Shift + Mid-Drag : Rotate Axis"); ty += step;
    drawStr(tx, ty, "  Wheel : Zoom In / Out"); ty += step;
    drawStr(tx, ty, "  R Key : Reset View"); ty += step; ty += half_step;
    
    // 2. ツール操作 (UIタブと連動)
    glColor3f(0.7f, 1.0f, 0.7f); drawStr(tx, ty, "[ Tools (Select via Tabs) ]"); ty += step;
    glColor3f(1,1,1);
    drawStr(tx, ty, "  Brush / Eraser : Left Drag (Draw) / Right Drag (Erase)"); ty += step;
    drawStr(tx, ty, "  Lasso Tool     : Left Drag (Select) / Right Drag (Move)"); ty += step;
    drawStr(tx, ty, "  Flood Fill     : Left Click (Fill) / Right Click (Erase Fill)"); ty += step;
    drawStr(tx, ty, "  Color Picker   : Left Click (Pick Color)"); ty += step; ty += half_step;
    
    // 3. ショートカットキー群
    glColor3f(0.9f, 0.8f, 1.0f); drawStr(tx, ty, "[ Shortcuts ]"); ty += step;
    glColor3f(1,1,1);
    drawStr(tx, ty, "  F / D Key : Temp Flood Fill / Color Picker"); ty += step;
    drawStr(tx, ty, "  Ctrl+Z / Ctrl+Y : Undo / Redo"); ty += step;
    drawStr(tx, ty, "  Ctrl+S : Save Project"); ty += step;
    drawStr(tx, ty, "  I Key  : Import Image (as Lasso)"); ty += step;
    drawStr(tx, ty, "  L / Shift+L : Load / Unload Middle Layer"); ty += step;
    drawStr(tx, ty, "  Shift + R Key : Clear Canvas"); ty += step;
    drawStr(tx, ty, "  Shift + ESC : Exit Program"); ty += step; ty += step;
    
    // 閉じる案内
    glColor3f(1.0f, 1.0f, 0.0f); drawStr(tx, ty, "Click 'Help' button again to close.");
    
    // スクロールの最大値をテキストの最終位置から計算
    float final_y_pos = (ty + step + menu_scroll_y);
    float view_bottom_y = (float)(win_h - bottom_margin);
    max_scroll_y = std::max(0.0f, (final_y_pos + 50) - view_bottom_y);
    glDisable(GL_SCISSOR_TEST);
}
