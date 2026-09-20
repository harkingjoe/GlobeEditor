/*Lassoツールロジックの管理*/

#ifndef __LASSOTOOL_H__
#define __LASSOTOOL_H__


#include "MathUtils.h"
#include <vector>

// 前方宣言（ポインタとして受け取るため、ヘッダのインクルードを避ける）
class LayerManager;
class Camera;

// 3D点群用構造体
typedef struct {
    Vec3 p;                // 3D位置
    unsigned char r, g, b; // 色
} CloudPoint;

class LassoTool {
private:
    bool is_active;          // Lassoモードかどうか (旧: is_lasso_mode)
    bool is_selecting;       // 選択の枠線を描いている途中か
    bool has_floating;       // 切り取られた浮遊点群があるか

    std::vector<Vec3> lasso_path_3d; // 枠線の3D座標
    std::vector<UVPoint> lasso_path; // UI表示用(スクリーン座標)
    
    std::vector<CloudPoint> points;  // レイヤー全体の点群
    std::vector<CloudPoint> floats;  // 切り取られた浮遊点群

public:
    LassoTool();
    ~LassoTool();

    // モード切替
    void enterMode(LayerManager* layerMgr);
    void exitMode(LayerManager* layerMgr);
    bool isActive() const { return is_active; }
    bool hasFloating() const { return has_floating; }

    // マウス・キー操作によるアクション
    void startSelection(int mx, int my);
    void recordSelection(int mx, int my);
    void cutSelection(Camera* camera);
    void scaleSelection(float scale_factor, Camera* camera);
    void moveSelection(int dx, int dy, Camera* camera);
    void rotateSelection(int last_x, int last_y, int x, int y, Camera* camera); // ★ 追加
    void pasteSelection();
    void updateScreenPath(Camera* camera); // ★ 追加

    // 外部から画像をインポートしたときの処理（旧: imp_img の一部）
    void importFloatingPoints(const std::vector<CloudPoint>& new_floats);

    // 描画処理 (display関数から呼ばれる)
    void draw(Camera* camera, int win_w, int win_h);

private:
    // 内部処理用
    void rasterizePoints(LayerManager* layerMgr);
    bool isInPolyScreen(float x, float y) const;
    void drawLassoLine(int win_w, int win_h) const;
};

#endif
