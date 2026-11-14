# SPDX-License-Identifier: GPL-2.0-or-later
"""Implementação de referência (independente do C) das provas Gráficos 2D do SaguiMark (D102).
Uso: python3 tests/saguimark_ref2d.py  -> imprime os valores esperados que ficam em userland/libsagui/saguimark.c."""
M=0xFFFFFFFF
class R:
    def __init__(s,seed): s.x=seed
    def __call__(s):
        x=s.x; x^=(x<<13)&M; x^=x>>17; x^=(x<<5)&M; s.x=x; return x
W=128
def novo(): return [0]*(W*W)
def ponto(px,x,y,c):
    if 0<=x<W and 0<=y<W: px[y*W+x]=c
def linha(px,x0,y0,x1,y1,c):
    dx=abs(x1-x0); sx=1 if x0<x1 else -1
    dy=abs(y1-y0); sy=1 if y0<y1 else -1
    passos=max(dx,dy); acum=0
    for k in range(passos+1):
        ponto(px,x0,y0,c)
        if dx>=dy:
            x0+=sx; acum+=dy
            if 2*acum>=dx: y0+=sy; acum-=dx
        else:
            y0+=sy; acum+=dx
            if 2*acum>=dy: x0+=sx; acum-=dy
def soma(px):
    h=2166136261
    for v in px: h=((h^v)*16777619)&M
    return h
def linhas(n,seed):
    r=R(seed); px=novo()
    for i in range(n):
        x0=r()%160-16; y0=r()%160-16; x1=r()%160-16; y1=r()%160-16; c=r()
        linha(px,x0,y0,x1,y1,c)
    return soma(px)
def rets(n,seed):
    r=R(seed); px=novo(); tot=0
    for i in range(n):
        x=r()%160-16; y=r()%160-16; l=r()%128+1; a=r()%128+1; c=r()
        x1=min(x+l,W); y1=min(y+a,W); x=max(x,0); y=max(y,0)
        if x>=x1: continue
        for yy in range(y,y1):
            px[yy*W+x:yy*W+x1]=[c]*(x1-x)
        tot+=(x1-x)*max(0,y1-y)
    return soma(px),tot
def copiar(px,sx,sy,l,a,dx,dy):
    if dx<0: l+=dx; sx-=dx; dx=0
    if dy<0: a+=dy; sy-=dy; dy=0
    if sx<0: l+=sx; dx-=sx; sx=0
    if sy<0: a+=sy; dy-=sy; sy=0
    if dx+l>W: l=W-dx
    if sx+l>W: l=W-sx
    if dy+a>W: a=W-dy
    if sy+a>W: a=W-sy
    if l<=0 or a<=0: return 0
    rows=range(a-1,-1,-1) if dy>sy else range(a)
    for y in rows:
        px[(dy+y)*W+dx:(dy+y)*W+dx+l]=px[(sy+y)*W+sx:(sy+y)*W+sx+l]
    return l*a
def blits(n,seed):
    r=R(seed); px=[(i*2654435761)&M for i in range(W*W)]; tot=0
    for i in range(n):
        l=r()%64+32; a=r()%64+32
        sx=r()%(W-l+1); sy=r()%(W-a+1); dx=r()%(W-l+1); dy=r()%(W-a+1)
        tot+=copiar(px,sx,sy,l,a,dx,dy)
    return soma(px),tot
# Cargas de userland/libsagui/saguimark.c (LIN_N, PAN_N, RET_N, BLT_N) e sementes.
LIN_N, PAN_N, PAN_CADA, RET_N, BLT_N = 200000, 12000, 10, 60000, 20000

def esperados():
    """-> (soma linhas, soma linhas a tela, (soma rects, pixels), (soma blocos, pixels))"""
    return linhas(LIN_N, 31337), linhas(PAN_N, 2718), rets(RET_N, 4711), blits(BLT_N, 1618)

if __name__ == "__main__":
    a, b, (c, cp), (d, dp) = esperados()
    print("lin %#x\npan %#x\nret %#x %d\nblt %#x %d" % (a, b, c, cp, d, dp))
