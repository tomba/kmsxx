#!/bin/sh

uncrustify -c uncrustify.cfg -l CPP --no-backup $(git ls-files '*.cpp' '*.h')
