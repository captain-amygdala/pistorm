vc a314fs.c -S -o a314fs.asm
python comment_out_sections.py
vc bcpl_start.asm a314fs.asm bcpl_end.asm -nostdlib -o ../a314fs
python patch_a314fs.py
del a314fs.asm
