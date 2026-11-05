/*各クラスのインスタンスを保持し、処理を統括する。GLUTからのコールバックを受け取る窓口*/

#ifndef __GLOBEEDITORAPP_H_
#define __GLOBEEDITORAPP_H_


// 前方宣言
class LayerManager;
class HistoryManager;
class Camera;
class UIManager;
class LassoTool;
class BrushTool;

class GlobeEditorApp {
private:
    // --- モジュール群 ---
    LayerManager* layerMgr;
    HistoryManager* historyMgr;
    Camera* camera;
    UIManager* uiMgr;
    LassoTool* lassoTool;
    BrushTool* brushTool;

    // --- アプリケーションの状態 ---
    int win_w, win_h;
    int last_x, last_y;
    int btn_mask; // 押下中のマウスボタン (1 << GLUT_xxx_BUTTON のビットの組み合わせ)
    
    bool is_sft;  // Shiftキーが押されているか
    bool is_rot;  // 中ボタンで回転中か
    bool is_drw;  // 左/右クリックで描画中か
    bool is_ers;  // 右クリック(消しゴム)中か
    bool key_f;   // Fキー(塗りつぶし)
    bool key_d;   // Dキー(スポイト)
    bool key_c;   // Cキー
    
    bool tex_dirty; // テクスチャの更新が必要かフラグ

    // --- ファイルパス設定 ---
    const char* MID_F    = "map_middle.PNG";
    const char* INS_F    = "insert.PNG";
    const char* LYR_F    = "saved_layer.png";
    const char* OUT_FILE = "map_edited.png";

public:
    static const char* const IN_FILE; // 背景画像

    GlobeEditorApp(int w, int h);
    ~GlobeEditorApp();

    // ウィンドウを開く前に背景画像を読めるか確認する
    static bool canLoadBaseImage();

    // 初期化
    void init();

    // GLUTコールバックに対応する処理
    void display();
    void idle();
    void reshape(int w, int h);
    void mouse(int btn, int state, int x, int y);
    void motion(int x, int y);
    void wheel(int whl, int dir, int x, int y);
    void keyboard(unsigned char key, int x, int y);
    void keyboardUp(unsigned char key, int x, int y);

    // タイマーコールバック用
    void performSaveTask(int type); // 0=Project Save, 1=Export

private:
    // 内部ヘルパー
    void checkToolSwitch();
    void importImage();
    void calculateAreaRatio(); // ★ 追加
};

#endif
