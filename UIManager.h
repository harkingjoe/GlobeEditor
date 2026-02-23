/*画面上のボタン、メニュー、文字の描画と、UIに対するクリック判定*/

#ifndef __UIMANAGER_H__
#define __UIMANAGER_H__


#include <string>
#include <GL/freeglut.h>

// UI操作対象の識別子
enum UITarget {
    TGT_NONE = -1,
    TGT_BRUSH, TGT_ERASER, TGT_LASSO,
    TGT_FILL, TGT_PICKER,
    TGT_R, TGT_G, TGT_B
};

class UIManager {
private:
    // --- UIの状態（ステート） ---
    UITarget current_tool;
    UITarget active_target;
    std::string input_str;

    float br_sz_drw;
    float br_sz_ers;
    unsigned char COL[3];

    int font_idx;
    int sens_idx;
    float rot_sens;

    bool show_menu;
    float menu_scroll_y;
    float max_scroll_y;

    // --- メッセージ用 ---
    int save_msg_timer;
    bool is_saving;
    bool is_exporting;
    int last_msg_type; // 0=None, 1=Save(Project), 2=Export

public:
    UIManager();
    ~UIManager();

    // 描画処理（display関数から呼ばれる）
    // ※Lassoの状態(is_lasso_mode, has_floating)だけ外部から教えてもらう
    void draw(int win_w, int win_h, bool is_lasso_mode, bool has_floating);

    // --- 入力処理（イベントを処理したら true を返す） ---
    int handleMouse(int btn, int state, int x, int y, int win_w, int win_h, bool is_lasso_mode);
    bool handleWheel(int dir, int x, int y, int win_h, bool is_lasso_mode);
    bool handleKeyboard(unsigned char key);

    // --- メッセージ表示制御 ---
    void startSaveMessage();
    void startExportMessage();
    void finishMessage(int type); // type: 1=Save, 2=Export
    void updateMessageTimer();

    // --- 外部からUIの状態を取得するためのゲッター ---
    UITarget getCurrentTool() const { return current_tool; }
    float getDrawBrushSize() const { return br_sz_drw; }
    float getEraseBrushSize() const { return br_sz_ers; }
    const unsigned char* getColor() const { return COL; }
    float getRotSens() const { return rot_sens; }
    bool isMenuOpen() const { return show_menu; }
    void closeMenu() { show_menu = false; }
    
    // 現在のツールを外部から強制変更する（Lassoの確定時など）
    void setCurrentTool(UITarget tool) { current_tool = tool; }

    // スポイト機能で取得した色をUIに反映させる
    void setColor(unsigned char r, unsigned char g, unsigned char b) {
        COL[0] = r; COL[1] = g; COL[2] = b;
    }

private:
    // 内部描画ヘルパー
    void drawMenu(int win_w, int win_h);
    void drawStr(float x, float y, const char* str);
    void drawCirc(float x, float y, float rad);
    void drawCircCol(float x, float y, float rad, const unsigned char* col);
    void drawStrOutline(float x, float y, const char* str, float alpha);
    
    void* getFont();
    int getLineStep();
};

#endif
