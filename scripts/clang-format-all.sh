#!/bin/sh

dirs="kms++ kmscube kms++util py utils"
find $dirs \( -name "*.cpp" -o -name "*.h" \) -exec clang-format -i {} \;
