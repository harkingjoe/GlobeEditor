CC:=gcc
TARGET:=Globe01

Globe01: GlobeEditor01-main01.cpp
	g++ $< -o $@ -lGL -lGLU -lglut -lm