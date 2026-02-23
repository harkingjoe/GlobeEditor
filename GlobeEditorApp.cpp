#include "GlobeEditorApp.h"
#include "LayerManager.h"
#include "HistoryManager.h"
#include "Camera.h"
#include "UIManager.h"
#include "LassoTool.h"
#include "BrushTool.h"

#include <GL/freeglut.h>
#include "stb_image.h" // 画像インポートのピクセル読み取り用（※実装マクロは書かない）

#include <cstring>  // memset用
#include <cstdlib>  // exit(0)用
#include <vector>   // std::vector用


GlobeEditorApp::GlobeEditorApp(int w, int h) : win_w(w), win_h(h) {
    layerMgr   = new LayerManager();
    historyMgr = new HistoryManager(w, h);
    camera     = new Camera(w, h);
    uiMgr      = new UIManager();
    lassoTool  = new LassoTool();
    brushTool  = new BrushTool();

    last_x = 0; last_y = 0; cur_btn = -1;
    is_sft = is_rot = is_drw = is_ers = false;
    key_f = key_d = key_c = false;
    tex_dirty = false;
}

GlobeEditorApp::~GlobeEditorApp() {
    delete layerMgr;
    delete historyMgr;
    delete camera;
    delete uiMgr;
    delete lassoTool;
    delete brushTool;
}

void GlobeEditorApp::init() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1.0f, 1.0f, 1.0f);

    // 画像とレイヤーの読み込み
    layerMgr->loadBaseImage(IN_FILE);
    layerMgr->loadLayer(LYR_F);
    
    // HistoryManager に正しいデータサイズを知らせるため再確保
    delete historyMgr;
    historyMgr = new HistoryManager(layerMgr->getWidth(), layerMgr->getHeight());

    tex_dirty = true;
}

void GlobeEditorApp::display() {
    // 1. 必要に応じてOpenGLのテクスチャを更新
    if (tex_dirty) {
        layerMgr->updateTexture();
        tex_dirty = false;
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 2. カメラの行列をセット
    glMatrixMode(GL_MODELVIEW);
    camera->applyTransform();
    
    // 3. 地球儀（背景＋レイヤー合成結果）の描画
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, layerMgr->getTextureID());
    glColor3f(1.0f, 1.0f, 1.0f);
    
    GLUquadric *q = gluNewQuadric();
    gluQuadricTexture(q, GL_TRUE);
    gluSphere(q, SPH_RAD - 0.01f, 64, 64);
    gluDeleteQuadric(q);
    
    // 4. ツールごとの追加描画 (Lassoの点群など)
    lassoTool->draw(camera, win_w, win_h);
    
    // 5. UIの描画 (画面の手前に2Dで描画)
    uiMgr->draw(win_w, win_h, lassoTool->isActive(), lassoTool->hasFloating());
    
    glutSwapBuffers();
}

void GlobeEditorApp::idle() {
    uiMgr->updateMessageTimer();
    
    if (camera->isResetting()) {
        camera->updateResetAnimation();
        glutPostRedisplay();
    } else if (uiMgr->isMenuOpen() == false) {
        // メッセージのフェードアウト等があるため、常に再描画をかけるか判断
        glutPostRedisplay(); 
    }
}

void GlobeEditorApp::reshape(int w, int h) {
    win_w = w; win_h = h;
    camera->updateWindowSize(w, h);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

// ---------------------------------------------------------
// 入力イベント（UI -> ツール の順で処理）
// ---------------------------------------------------------
void GlobeEditorApp::checkToolSwitch() {
    UITarget current_ui_tool = uiMgr->getCurrentTool();
    
    // UIでLassoが選ばれたが、まだLassoモードに入っていない場合
    if (current_ui_tool == TGT_LASSO && !lassoTool->isActive()) {
        historyMgr->push_und(layerMgr->getLayerBuffer());
        lassoTool->enterMode(layerMgr);
        tex_dirty = true;
    } 
    // UIでブラシ・消しゴムが選ばれたが、まだLassoモードにいる場合
    else if (current_ui_tool != TGT_LASSO && lassoTool->isActive()) {
        lassoTool->exitMode(layerMgr);
        tex_dirty = true;
    }
}

void GlobeEditorApp::mouse(int btn, int state, int x, int y) {
    if (state == GLUT_DOWN) cur_btn = btn; else if (state == GLUT_UP) cur_btn = -1;
    is_sft = (glutGetModifiers() & GLUT_ACTIVE_SHIFT);

    if (state == GLUT_DOWN) camera->stopReset();

    // 1. UIにクリックを渡す
    if (uiMgr->handleMouse(btn, state, x, y, win_w, win_h, lassoTool->isActive())) {
        checkToolSwitch(); // タブが切り替わったか確認
        glutPostRedisplay();
        return;
    }

    // UI以外をクリックした場合
    if (lassoTool->isActive()) {
        if (btn == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) {
                if (lassoTool->hasFloating()) {
                    lassoTool->pasteSelection();
                    // 確定したらブラシモードに戻す
                    lassoTool->exitMode(layerMgr);
                    uiMgr->setCurrentTool(TGT_BRUSH);
                    tex_dirty = true;
                } else {
                    lassoTool->startSelection(x, y);
                }
            } else if (state == GLUT_UP) {
                lassoTool->cutSelection(camera);
                tex_dirty = true;
            }
        }
    } else {
        // ブラシモード
        if (state == GLUT_DOWN && (btn == GLUT_LEFT_BUTTON || btn == GLUT_RIGHT_BUTTON)) {
            historyMgr->push_und(layerMgr->getLayerBuffer());
            brushTool->resetStroke(); // 描画開始時にストロークをリセット
        }
        
        if (btn == GLUT_MIDDLE_BUTTON) {
            is_rot = (state == GLUT_DOWN);
        } else if (btn == GLUT_LEFT_BUTTON) {
            is_drw = (state == GLUT_DOWN); is_ers = false;
            if (state == GLUT_DOWN) {
                if (key_f) { brushTool->floodFill(x, y, camera, layerMgr, uiMgr, historyMgr, false); is_drw = false; tex_dirty = true; }
                else if (key_d) { brushTool->pickColor(x, y, camera, layerMgr, uiMgr); is_drw = false; }
            }
            if (is_drw) { brushTool->paint(x, y, camera, layerMgr, uiMgr, false); tex_dirty = true; }
        } else if (btn == GLUT_RIGHT_BUTTON) {
            is_drw = (state == GLUT_DOWN); is_ers = true;
            if (state == GLUT_DOWN && key_f) { brushTool->floodFill(x, y, camera, layerMgr, uiMgr, historyMgr, true); is_drw = false; tex_dirty = true; }
            else if (is_drw) { brushTool->paint(x, y, camera, layerMgr, uiMgr, true); tex_dirty = true; }
        }
    }
    
    last_x = x; last_y = y;
    if (state == GLUT_DOWN || state == GLUT_UP) glutPostRedisplay();
}

void GlobeEditorApp::motion(int x, int y) {
    if (uiMgr->isMenuOpen()) return;

    int dx = x - last_x; int dy = y - last_y;

    if (lassoTool->isActive()) {
        if (!lassoTool->hasFloating() && cur_btn == GLUT_LEFT_BUTTON) {
            lassoTool->recordSelection(x, y);
        } else if (lassoTool->hasFloating() && cur_btn == GLUT_RIGHT_BUTTON) {
            lassoTool->moveSelection(dx, dy, camera);
        } else if (cur_btn == GLUT_MIDDLE_BUTTON || (cur_btn == GLUT_LEFT_BUTTON && is_sft)) {
            camera->rotateView(dx, dy, uiMgr->getRotSens());
        }
    } else {
        if (is_rot || (is_sft && cur_btn == GLUT_MIDDLE_BUTTON)) {
            // トラックボール回転
            camera->rotateView(dx, dy, uiMgr->getRotSens());
        } else if (is_drw) {
            brushTool->paint(x, y, camera, layerMgr, uiMgr, is_ers);
            tex_dirty = true;
        }
    }

    last_x = x; last_y = y;
    glutPostRedisplay();
}

void GlobeEditorApp::wheel(int whl, int dir, int x, int y) {
    if (uiMgr->handleWheel(dir, x, y, win_h, lassoTool->isActive())) {
        glutPostRedisplay(); return;
    }

    if (lassoTool->isActive() && lassoTool->hasFloating()) {
        float s = (dir > 0) ? 0.95f : 1.05f;
        lassoTool->scaleSelection(s, camera);
        glutPostRedisplay();
        return;
    }

    // カメラズーム
    float zoom_delta = (dir > 0) ? 0.5f : -0.5f;
    camera->addCamDist(zoom_delta);
    glutPostRedisplay();
}

void GlobeEditorApp::keyboard(unsigned char key, int x, int y) {
    if (key == 'f' || key == 'F') { key_f = true; return; }
    if (key == 'd' || key == 'D') { key_d = true; return; }
    if (key == 'c' || key == 'C') { key_c = true; glutPostRedisplay(); return; }

    camera->stopReset();

    // UIへ入力文字を渡す
    if (uiMgr->handleKeyboard(key)) {
        glutPostRedisplay(); return;
    }

    // ショートカットキー
    if (key == 'i' || key == 'I') { importImage(); return; }
    if (key == 'l' || key == 'L') { 
        if(layerMgr->loadMiddleLayer(MID_F)) tex_dirty = true; 
        return; 
    }

    if (key == 27) { // ESC
        if (uiMgr->isMenuOpen()) uiMgr->closeMenu();
        else if (lassoTool->isActive()) {
            if (lassoTool->hasFloating()) { 
                lassoTool->pasteSelection(); 
            } else { 
                lassoTool->exitMode(layerMgr); 
                uiMgr->setCurrentTool(TGT_BRUSH); 
            }
            tex_dirty = true;
        } else if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
            exit(0);
        }
        glutPostRedisplay();
        return;
    }
    
    if (key == 26) { // Ctrl+Z
        if(historyMgr->undo(layerMgr->getLayerBuffer())) {
            layerMgr->updateCompositeAll(); tex_dirty = true;
        }
    }
    else if (key == 25) { // Ctrl+Y
        if(historyMgr->redo(layerMgr->getLayerBuffer())) {
            layerMgr->updateCompositeAll(); tex_dirty = true;
        }
    }
    else if (key == 'r') { camera->startReset(); }
    else if (key == 'R') {
        historyMgr->push_und(layerMgr->getLayerBuffer());
        std::memset(layerMgr->getLayerBuffer(), 0, layerMgr->getWidth() * layerMgr->getHeight() * 4);
        layerMgr->updateCompositeAll(); tex_dirty = true;
    }
    
    glutPostRedisplay();
}

void GlobeEditorApp::keyboardUp(unsigned char key, int x, int y) {
    if (key == 'f' || key == 'F') key_f = false;
    if (key == 'd' || key == 'D') key_d = false;
    if (key == 'c' || key == 'C') { key_c = false; glutPostRedisplay(); }
}

// ---------------------------------------------------------
// 高度な処理 (保存・インポート)
// ---------------------------------------------------------

void GlobeEditorApp::importImage() {
    int w, h, c;
    unsigned char* dat = stbi_load(INS_F, &w, &h, &c, 4);
    if (!dat) return;

    if (!lassoTool->isActive()) {
        lassoTool->enterMode(layerMgr);
        uiMgr->setCurrentTool(TGT_LASSO);
    }
    
    std::vector<CloudPoint> floats;
    float sc_u = 0.2f; 
    float sc_v = sc_u * 2.0f * ((float)h / w); 

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 4;
            if (dat[idx+3] < 10) continue;

            float nx = (float)x / w - 0.5f;
            float ny = (float)y / h - 0.5f;
            float u = 0.5f + nx * sc_u;
            float v = 0.5f - ny * sc_v; 
            
            if (u >= 0 && u <= 1 && v >= 0 && v <= 1) {
                CloudPoint cp;
                cp.p = uv2xyz(u, v, SPH_RAD);
                cp.r = dat[idx]; cp.g = dat[idx+1]; cp.b = dat[idx+2];
                floats.push_back(cp);
            }
        }
    }
    stbi_image_free(dat);
    
    lassoTool->importFloatingPoints(floats);
    tex_dirty = true;
    glutPostRedisplay();
}

void GlobeEditorApp::performSaveTask(int type) {
    bool was_lasso = false;
    if (lassoTool->isActive()) {
        lassoTool->exitMode(layerMgr);
        was_lasso = true;
    }

    if (type == 0) {
        layerMgr->saveLayer(LYR_F);
        uiMgr->finishMessage(1);
    } else {
        layerMgr->exportComposite(OUT_FILE);
        uiMgr->finishMessage(2);
    }

    if (was_lasso) lassoTool->enterMode(layerMgr);
    glutPostRedisplay();
}
