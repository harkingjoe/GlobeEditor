// SPDX-FileCopyrightText: 2026 harkingjoe
// SPDX-License-Identifier: MIT
#include "HistoryManager.h"
#include <cstdlib>
#include <cstring>

// コンストラクタ（初期化）
HistoryManager::HistoryManager(int img_w, int img_h) {
    data_size = img_w * img_h * 4;
}

// デストラクタ（破棄時にメモリを解放）
HistoryManager::~HistoryManager() {
    clean_hist();
}

void HistoryManager::push_und(const unsigned char* current_layer) {
    if (!current_layer) return;

    // 新しい状態を保存するためのメモリを確保してコピー
    unsigned char* snap = (unsigned char*)std::malloc(data_size);
    if (!snap) return;
    std::memcpy(snap, current_layer, data_size); 
    
    u_stk.push_back(snap);
    
    // 上限を超えたら古いものから消す
    if (u_stk.size() > MAX_HIST) {
        std::free(u_stk.front());
        u_stk.erase(u_stk.begin());
    }
    
    // 新しい操作をしたのでRedo履歴は消去する
    for (auto ptr : r_stk) std::free(ptr);
    r_stk.clear();
}

bool HistoryManager::undo(unsigned char* current_layer) {
    if (u_stk.empty()) return false;

    // 現在の状態をRedoスタックに退避
    unsigned char* cur_dat = (unsigned char*)std::malloc(data_size);
    std::memcpy(cur_dat, current_layer, data_size);
    r_stk.push_back(cur_dat);
    
    // Undoスタックから1つ前の状態を取り出して現在のレイヤーに上書き
    unsigned char* prv_dat = u_stk.back();
    u_stk.pop_back();
    std::memcpy(current_layer, prv_dat, data_size);
    std::free(prv_dat);
    
    return true; // Undo成功
}

bool HistoryManager::redo(unsigned char* current_layer) {
    if (r_stk.empty()) return false;

    // 現在の状態をUndoスタックに退避
    unsigned char* cur_dat = (unsigned char*)std::malloc(data_size);
    std::memcpy(cur_dat, current_layer, data_size);
    u_stk.push_back(cur_dat);
    
    // Redoスタックから未来の状態を取り出して上書き
    unsigned char* nxt_dat = r_stk.back();
    r_stk.pop_back();
    std::memcpy(current_layer, nxt_dat, data_size);
    std::free(nxt_dat);
    
    return true; // Redo成功
}

void HistoryManager::clean_hist() {
    for (auto ptr : u_stk) std::free(ptr);
    u_stk.clear();
    
    for (auto ptr : r_stk) std::free(ptr);
    r_stk.clear();
}