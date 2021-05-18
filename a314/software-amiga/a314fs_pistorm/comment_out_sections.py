with open('a314fs.asm', 'r+b') as f:
    text = f.read().decode('utf-8')
    text = text.replace('section', ';section')
    f.seek(0, 0)
    f.write(text.encode('utf-8'))
