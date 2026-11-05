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
#include <cstdio>   // fprintf用
#include <vector>   // std::vector用


extern void timerSaveCallback(int value);

const char* const GlobeEditorApp::IN_FILE = "map_initial.PNG";

bool GlobeEditorApp::canLoadBaseImage() {
    int w, h, c;
    return stbi_info(IN_FILE, &w, &h, &c) != 0;
}

GlobeEditorApp::GlobeEditorApp(int w, int h) : win_w(w), win_h(h) {
    layerMgr   = new LayerManager();
    historyMgr = new HistoryManager(w, h);
    camera     = new Camera(w, h);
    uiMgr      = new UIManager();
    lassoTool  = new LassoTool();
    brushTool  = new BrushTool();

    last_x = 0; last_y = 0; btn_mask = 0;
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
    // canLoadBaseImage() はヘッダーしか見ないため、壊れた画像はここで検出する
    if (!layerMgr->loadBaseImage(IN_FILE)) {
        std::fprintf(stderr, "Failed to load base image: %s\n", IN_FILE);
        std::exit(EXIT_FAILURE);
    }
    layerMgr->loadLayer(LYR_F);
    
    // HistoryManager に正しいデータサイズを知らせるため再確保
    delete historyMgr;
    historyMgr = new HistoryManager(layerMgr->getWidth(), layerMgr->getHeight());

    // ★ 初期ロード時にも面積比を計算してUIに渡す
    calculateAreaRatio();

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
    lassoTool->draw();
    
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
    // 複数ボタンの同時押し (左ドラッグ中のホイールドラッグ等) に対応するため、ビットで管理する
    if (btn >= 0 && btn < 3) {
        if (state == GLUT_DOWN) btn_mask |= (1 << btn);
        else if (state == GLUT_UP) btn_mask &= ~(1 << btn);
    }
    is_sft = (glutGetModifiers() & GLUT_ACTIVE_SHIFT);

    if (state == GLUT_DOWN) camera->stopReset();

    // 1. UIにクリックを渡す
    int ui_res = uiMgr->handleMouse(btn, state, x, y, win_w, win_h, lassoTool->isActive());
    if (ui_res == 2) {
        // Exportボタン処理
        uiMgr->startExportMessage();
        glutTimerFunc(50, timerSaveCallback, 1);
        glutPostRedisplay();
        return;
    } else if (ui_res == 1) {
        // 通常のUI処理
        checkToolSwitch();
        glutPostRedisplay();
        return;
    }

    // UI以外をクリックした場合
    if (lassoTool->isActive()) {
        if (btn == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) {
                if (lassoTool->hasFloating()) {
                    /* lassoTool->pasteSelection();
                    // 確定したらブラシモードに戻す
                    lassoTool->exitMode(layerMgr);
                    uiMgr->setCurrentTool(TGT_BRUSH);
                    tex_dirty = true; */
                } else if (!is_sft) {
                    // Shift+左ドラッグは視点回転なので選択を開始しない
                    lassoTool->startSelection(x, y, camera);
                }
            } else if (state == GLUT_UP) {
                // ★ 修正: 選択中の時だけ cutSelection を呼ぶ
                if (lassoTool->isSelecting()) { 
                    lassoTool->cutSelection();
                    tex_dirty = true;
                }
            }
        }
    } else {
        // ブラシモード
        if (state == GLUT_DOWN && (btn == GLUT_LEFT_BUTTON || btn == GLUT_RIGHT_BUTTON)) {
            historyMgr->push_und(layerMgr->getLayerBuffer());
            brushTool->resetStroke(); 
        }
        
        if (btn == GLUT_MIDDLE_BUTTON) {
            is_rot = (state == GLUT_DOWN);
        } else if (btn == GLUT_LEFT_BUTTON) {
            is_drw = (state == GLUT_DOWN); is_ers = false;
            UITarget cur_tool = uiMgr->getCurrentTool(); // ★現在のツールを取得

            if (state == GLUT_DOWN) {
                // ★Fキー、またはUIで「Flood Fill」が選ばれている場合
                if (key_f || cur_tool == TGT_FILL) { 
                    brushTool->floodFill(x, y, camera, layerMgr, uiMgr, historyMgr, false); 
                    is_drw = false; tex_dirty = true; 
                }
                // ★Dキー、またはUIで「Color Picker」が選ばれている場合
                else if (key_d || cur_tool == TGT_PICKER) { 
                    brushTool->pickColor(x, y, camera, layerMgr, uiMgr); 
                    is_drw = false; glutPostRedisplay(); 
                }
            }
            if (is_drw) { brushTool->paint(x, y, camera, layerMgr, uiMgr, false); tex_dirty = true; }
        } else if (btn == GLUT_RIGHT_BUTTON) {
            is_drw = (state == GLUT_DOWN); is_ers = true;
            UITarget cur_tool = uiMgr->getCurrentTool(); // ★現在のツールを取得

            // 右クリックでの塗りつぶし（透明化）もUIのFillツールに連動
            if (state == GLUT_DOWN && (key_f || cur_tool == TGT_FILL)) { 
                brushTool->floodFill(x, y, camera, layerMgr, uiMgr, historyMgr, true); 
                is_drw = false; tex_dirty = true; 
            }
            // スポイト中は右クリック描画を無効化
            else if (is_drw && cur_tool != TGT_PICKER) { 
                brushTool->paint(x, y, camera, layerMgr, uiMgr, true); tex_dirty = true; 
            }
        }
    }
    
    last_x = x; last_y = y;
    if (state == GLUT_DOWN || state == GLUT_UP) glutPostRedisplay();
}

void GlobeEditorApp::motion(int x, int y) {
    if (uiMgr->isMenuOpen()) return;

    int dx = x - last_x; int dy = y - last_y;

    if (lassoTool->isActive()) {
        bool btn_l = btn_mask & (1 << GLUT_LEFT_BUTTON);
        bool btn_m = btn_mask & (1 << GLUT_MIDDLE_BUTTON);
        bool btn_r = btn_mask & (1 << GLUT_RIGHT_BUTTON);

        // 視点回転 (ホイールドラッグ / Shift+左ドラッグ)
        // 枠線は球面上の3D座標で保持しているため、視点を動かしても地球上の位置からずれない
        bool rotating = btn_m || (btn_l && is_sft && !lassoTool->isSelecting());
        if (rotating) {
            camera->rotateView(dx, dy, uiMgr->getRotSens());
        }

        if (lassoTool->isSelecting()) {
            // 左ボタンを押したままなら、視点移動中も選択を継続する
            if (btn_l) lassoTool->recordSelection(x, y, camera);
        } else if (lassoTool->hasFloating() && !rotating) {
            if (btn_l) {
                lassoTool->moveSelection(dx, dy, camera);
            } else if (btn_r) {
                // ★ 引数を dx, dy から last_x, last_y, x, y に変更
                lassoTool->rotateSelection(last_x, last_y, x, y, camera);
            }
        }
    } else {
        if (is_rot) {
            if (is_sft) {
                // Shift押下時: 画面の軸を基準に全体を回転 (地軸ごと傾ける)
                camera->rotateView(dx, dy, uiMgr->getRotSens());
            } else {
                // 通常時: 地軸を固定して地球儀だけを自転させる
                camera->spinGlobe(last_x, last_y, x, y, uiMgr->getRotSens());
            }
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

    // 選択中にズームした場合は、カーソル位置までの枠線を継続する
    if (lassoTool->isActive() && lassoTool->isSelecting()) {
        lassoTool->recordSelection(x, y, camera);
    }
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

    // ★ 追加: Lassoモード中でEnterキーが押されたら確定
    if (key == 13) { 
        if (lassoTool->isActive() && lassoTool->hasFloating()) {
            lassoTool->pasteSelection();
            lassoTool->exitMode(layerMgr);
            uiMgr->setCurrentTool(TGT_BRUSH);
            tex_dirty = true;
            glutPostRedisplay();
        }
        return;
    }

    // ショートカットキー
    if (key == 'i' || key == 'I') { importImage(); return; }
    if (key == 'l') { // 小文字 (そのままLキー)
        if(layerMgr->loadMiddleLayer(MID_F)) tex_dirty = true; 
        return; 
    }
    if (key == 'L') { // 大文字 (Shift + Lキー)
        layerMgr->unloadMiddleLayer();
        tex_dirty = true;
        glutPostRedisplay();
        return; 
    }

    if (key == 19) { 
        uiMgr->startSaveMessage();
        glutTimerFunc(50, timerSaveCallback, 0);
        glutPostRedisplay();
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
        if (lassoTool->isActive()) lassoTool->exitMode(layerMgr);
        if (historyMgr->undo(layerMgr->getLayerBuffer())) {
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

    // ★ セーブ時に面積比を計算
    calculateAreaRatio();

    if (type == 0) {
        const bool saved = layerMgr->saveLayer(LYR_F);
        if (!saved) std::fprintf(stderr, "Failed to save: %s\n", LYR_F);
        uiMgr->finishMessage(1, saved);
    } else {
        const bool exported = layerMgr->exportComposite(OUT_FILE);
        if (!exported) std::fprintf(stderr, "Failed to export: %s\n", OUT_FILE);
        uiMgr->finishMessage(2, exported);
    }

    if (was_lasso) lassoTool->enterMode(layerMgr);
    glutPostRedisplay();
}

void GlobeEditorApp::calculateAreaRatio() {
    int w = layerMgr->getWidth();
    int h = layerMgr->getHeight();
    unsigned char* layer = layerMgr->getLayerBuffer();
    if (!layer || w == 0 || h == 0) return;

    double land_area = 0.0;
    double ocean_area = 0.0;

    for (int y = 0; y < h; y++) {
        // Y座標から緯度(lat)を求める。vは画像上端から下端へ 0.0 ~ 1.0
        float v = (y + 0.5f) / (float)h;
        float lat = (v - 0.5f) * M_PI;
        // 重み付けは緯度のコサイン
        double weight = std::cos(lat);

        int row_idx = y * w * 4;
        for (int x = 0; x < w; x++) {
            // アルファ値(A)を取得
            unsigned char alpha = layer[row_idx + x * 4 + 3];
            if (alpha > 0) {
                land_area += weight;   // 塗られている場合は陸地
            } else {
                ocean_area += weight;  // 透過部分は海洋
            }
        }
    }

    double total = land_area + ocean_area;
    if (total > 0.0) {
        float land_ratio = (float)((land_area / total) * 100.0);
        float ocean_ratio = (float)((ocean_area / total) * 100.0);
        uiMgr->setAreaRatio(land_ratio, ocean_ratio);
    }
}
