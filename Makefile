# SPDX-FileCopyrightText: 2026 harkingjoe
# SPDX-License-Identifier: MIT

# コンパイラの指定
CXX = g++

# コンパイルオプション (C++11標準を使用、最適化ON、警告表示)
CXXFLAGS = -std=c++11 -Wall -O2

# リンクするライブラリ (LinuxのOpenGL & GLUT用)
LDFLAGS = -lGL -lGLU -lglut -lm

# 実行ファイル名
TARGET = GlobeEditor

# コンパイル対象のソースファイル一覧
SRCS = main.cpp \
       GlobeEditorApp.cpp \
       MathUtils.cpp \
       HistoryManager.cpp \
       LayerManager.cpp \
       Camera.cpp \
       UIManager.cpp \
       LassoTool.cpp \
       BrushTool.cpp

# ソースファイル名からオブジェクトファイル名(.o)を生成
OBJS = $(SRCS:.cpp=.o)

# デフォルトのターゲット (make と打った時に実行される)
all: $(TARGET)

# 実行ファイルの生成ルール (リンク処理)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 各 .cpp から .o を作るルール (コンパイル処理)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 生成物の削除 (make clean と打った時に実行される)
clean:
	rm -f $(OBJS) $(TARGET)