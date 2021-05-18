with open('../a314fs', 'r+b') as f:
    f.seek(0x1c)
    b = f.read(4)
    f.seek(0x20)
    f.write(b)
