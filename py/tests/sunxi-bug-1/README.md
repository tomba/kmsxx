Test file: sunxi-bugs.py

Setup:
- OrangePI3 board, ARMBIAN 22.02 Bullseye (27 FEB 2022)
- Display 1920x1080@60HZ HDMI

Bug 1: Distorted UI plane when scaling factor changes in dynamic

Test cases:

1. Change sleep to `sleep=0.002`, run the test
Result: UI (3rd) plane is distorted starting from the TOP of the screen, while VI plane is fine

2. Change sleep to `sleep=0.014`, run the test
Result: UI (3rd) plane is distorted starting from the BOTTOM of the screen, while VI plane is fine


Bug 2: Corrupted UI when primary (UI) plane not FULL screen  
Should be fixed by:  
https://patchwork.kernel.org/project/dri-devel/patch/20220424162633.12369-9-samuel@sholland.org/

Test case:
1. Uncomment `primary_h = 1080/2` line
Result: Bottom half of the screen is not visible & artifacts in the bottom half in some cases
