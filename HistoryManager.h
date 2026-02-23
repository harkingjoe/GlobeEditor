/*Undo/Redo用のバッファスタック管理*/

#ifndef __HISTORYMANAGER_H__
#define __HISTORYMANAGER_H__


#include <vector>
#include <cstddef> // size_t用

class HistoryManager {
private:
    std::vector<unsigned char*> u_stk; // Undoスタック
    std::vector<unsigned char*> r_stk; // Redoスタック
    const size_t MAX_HIST = 20;
    size_t data_size; // 画像1枚分のバイト数 (width * height * 4)

public:
    // コンストラクタで管理する画像のサイズを教える
    HistoryManager(int img_w, int img_h);
    ~HistoryManager();

    // 現在のレイヤーの配列を渡して保存させる
    void push_und(const unsigned char* current_layer);
    
    // Undo実行。成功したら true を返す
    bool undo(unsigned char* current_layer);
    
    // Redo実行。成功したら true を返す
    bool redo(unsigned char* current_layer);
    
    // 履歴のクリア
    void clean_hist();
};

#endif
