# SPDX-License-Identifier: GPL-2.0-or-later
import io,re,subprocess
pat=re.compile(r'<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n',re.S)
uf=subprocess.run(['git','diff','--name-only','--diff-filter=U'],capture_output=True,text=True).stdout.split()
pend=[]
for p in uf:
    if not p.endswith('.md'): pend.append(p); continue
    s=io.open(p,encoding='utf-8').read()
    s=pat.sub(lambda m: m.group(1).rstrip('\n')+'\n'+('\n' if p.endswith('decisoes.md') or p=='README.md' else '')+m.group(2),s)
    if p.endswith('decisoes.md'):
        i=s.index('\n## D169.')
        cab,cola=s[:i+1],s[i+1:]
        partes=[x.rstrip('\n')+'\n' for x in re.split(r'(?m)^(?=## D1\d\d\. )',cola) if x.strip()]
        partes.sort(key=lambda x:int(re.match(r'## D(\d+)',x).group(1)))
        s=cab+'\n'.join(partes)
    io.open(p,'w',encoding='utf-8').write(s); subprocess.run(['git','add',p])
    print('resuelto',p)
print('PENDIENTES:',' '.join(pend) if pend else 'ninguno')
