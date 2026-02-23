/*Brushツールの管理*/

#ifndef __BRUSHTOOL_H__
#define __BRUSHTOOL_H__


#include "MathUtils.h"
#include <vector>

// 前方宣言
class LayerManager;
class Camera;
class HistoryManager;
class UIManager;

class BrushTool {
private:
    float last_u;
    float last_v;

public:
    BrushTool();
    ~BrushTool();

    // マウスクリック時にストロークをリセットする（線が飛ぶのを防ぐ）
    void resetStroke();

    // 通常のドラッグ描画（内部で点補間を行う）
    void paint(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr, bool is_erase);

    // 塗りつぶし（Fキー）
    void floodFill(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr, HistoryManager* histMgr, bool is_erase);

    // スポイト（Dキー）
    void pickColor(int mx, int my, Camera* camera, LayerManager* layerMgr, UIManager* uiMgr);

private:
    // 実際に1点の円を描画する内部関数
    void applyBrush(float u, float v, LayerManager* layerMgr, UIManager* uiMgr, bool is_erase);
};

#endif
