#pragma once

#include <raylib.h>
#include <string>
#include <vector>

class Widget {
   private:
    Widget();
    ~Widget();
};

enum class Direction { Row, RowReverse, Column, ColumnReverse };

enum class Justify {
    Start,
    End,
    Center,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

class Panel : Widget {
   public:
    Panel(Direction direction, Justify justify);
    ~Panel();

   private:
    std::vector<Widget> children;
};

class Button : Widget {
   public:
    Button(std::string text);
    ~Button();
};

class TextBox : Widget {
   public:
    TextBox();
    ~TextBox();
};