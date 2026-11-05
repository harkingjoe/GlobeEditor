#include "GlobeEditorApp.h"
#include <GL/freeglut.h>
#include <cstdio>

// アプリケーション全体を管理するグローバルポインタ
GlobeEditorApp* g_app = nullptr;

// ---------------------------------------------------------
// GLUT用 ラッパー関数群
// ---------------------------------------------------------
void display() { g_app->display(); }
void idle()    { g_app->idle(); }
void reshape(int w, int h) { g_app->reshape(w, h); }
void mouse(int b, int s, int x, int y) { g_app->mouse(b, s, x, y); }
void motion(int x, int y)  { g_app->motion(x, y); }
void wheel(int w, int d, int x, int y) { g_app->wheel(w, d, x, y); }
void keyboard(unsigned char k, int x, int y) { g_app->keyboard(k, x, y); }
void keyboardUp(unsigned char k, int x, int y) { g_app->keyboardUp(k, x, y); }

// タイマー用の特殊なラッパー
void timerSaveCallback(int value) {
    g_app->performSaveTask(value);
}

// ---------------------------------------------------------
// メイン関数
// ---------------------------------------------------------
int main(int argc, char** argv) {
    // 背景画像がなければウィンドウを開かずに異常終了する
    if (!GlobeEditorApp::canLoadBaseImage()) {
        std::fprintf(stderr, "Base image not found or not readable: %s\n", GlobeEditorApp::IN_FILE);
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Globe Editor");

    // アプリケーションインスタンスの生成と初期化
    g_app = new GlobeEditorApp(800, 600);
    g_app->init();

    // コールバック関数の登録
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(wheel);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);

    // 今回はメニューからの「Save/Exportボタン」がUI側に組み込まれているため、
    // タイマーを利用した保存処理の登録例（Ctrl+Sなどが押された際にApp側から呼ぶ想定）
    // 例: g_app内で Saveフラグが立ったら glutTimerFunc(50, timerSaveCallback, 0); を呼ぶ
    // ※今回は同期処理に近い形でApp側で実行できるようにしています

    glutMainLoop();

    // 終了時のメモリ解放
    delete g_app;
    
    return 0;
}
