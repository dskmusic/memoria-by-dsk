import os
sp = os.path.dirname(os.path.abspath(__file__))
root = r"D:\Documentos\_Trabajo\DSK Music\_DSK Synths\Memoria by DSK\src\ui"
g = open(os.path.join(sp, 'room3d.glsl'), encoding='utf-8').read()
g = g.replace(')GLSL" R"GLSL(\n', '\n')
parts, cur = [], ''
for line in g.split('\n'):
    if len(cur) + len(line) > 12000:
        parts.append(cur); cur = ''
    cur += line + '\n'
parts.append(cur)
literal = 'const char* roomShader = R"GLSL(\n' + ')GLSL" R"GLSL(\n'.join(parts) + ')GLSL";\n'
print('pieces', [len(p) for p in parts])
p = os.path.join(root, 'GpuRoom.cpp')
b = open(p, 'rb').read().decode('utf-8'); crlf = '\r\n' in b
s = b.replace('\r\n', '\n')
a = s.index('const char* roomShader = R"GLSL('); e = s.index('const char* bloomShader')
s = s[:a] + literal + '\n' + s[e:]
open(p, 'wb').write((s.replace('\n', '\r\n') if crlf else s).encode('utf-8'))
print('ok', 'p.z + 3.0, 2.0' in g)
