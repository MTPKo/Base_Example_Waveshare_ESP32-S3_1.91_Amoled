import os
import shutil
Import("env")

# Brute force fix for LVGL 9 assembly compilation on ESP32-S3
# Deletes the Helium and Neon assembly folders that cause typedef errors on Xtensa
try:
    # Get the library dependencies directory
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    env_name = env.subst("$PIOENV")
    lvgl_dir = os.path.join(libdeps_dir, env_name, "lvgl")

    if os.path.exists(lvgl_dir):
        problematic_dirs = [
            os.path.join(lvgl_dir, "src", "draw", "sw", "blend", "helium"),
            os.path.join(lvgl_dir, "src", "draw", "sw", "blend", "neon"),
            os.path.join(lvgl_dir, "src", "draw", "convert", "helium"),
            os.path.join(lvgl_dir, "src", "draw", "convert", "neon"),
        ]
        
        for d in problematic_dirs:
            if os.path.exists(d):
                print(f"Removing problematic LVGL 9 directory: {d}")
                shutil.rmtree(d)
except Exception as e:
    print(f"Error in fix_lvgl_9.py: {e}")
