# usage: octgrams.py <iTime> <width> <height> <out.png>
# Direct numpy transcription of iOS/Reference/octgrams.glsl, float32.
import numpy as np, sys
from PIL import Image
f=np.float32
def rotmul(x,y,a):  # (x,y) * mat2(c,s,-s,c)
    c=np.cos(a).astype(f); s=np.sin(a).astype(f)
    return x*c+y*s, -x*s+y*c
def sdBox(px,py,pz,b):
    qx=np.abs(px)-f(b[0]); qy=np.abs(py)-f(b[1]); qz=np.abs(pz)-f(b[2])
    l=np.sqrt(np.maximum(qx,0)**2+np.maximum(qy,0)**2+np.maximum(qz,0)**2)
    return l+np.minimum(np.maximum(qx,np.maximum(qy,qz)),0)
def box(x,y,z,scale):
    return -(sdBox(x*scale,y*scale,z*scale,(.4,.4,.1))/f(1.5))
def box_set(x,y,z,g):
    s=np.sin(g*f(0.4)).astype(f); sc=f(2.)-np.abs(s)*f(1.5)
    r=[]
    for dx,dy in ((0,1),(0,-1),(1,0),(-1,0)):
        X,Y=rotmul(x+dx*s*f(2.5),y+dy*s*f(2.5),f(.8)); r.append(box(X,Y,z,sc))
    X,Y=rotmul(x,y,f(.8)); r.append(box(X,Y,z,f(.5))*f(6.))
    r.append(box(x,y,z,f(.5))*f(6.))
    m=r[0]
    for v in r[1:]: m=np.maximum(m,v)
    return m
def render(W,H,T):
    T=f(T)
    fx,fy=np.meshgrid(np.arange(W,dtype=f)+f(.5),np.arange(H,dtype=f)+f(.5))
    mn=f(min(W,H)); px=(fx*2-W)/mn; py=(fy*2-H)/mn
    n=np.sqrt(px*px+py*py+f(2.25)); rx,ry,rz=px/n,py/n,f(1.5)/n
    rx,ry=rotmul(rx,ry,np.sin(T*f(.03))*f(5.))
    ry,rz=rotmul(ry,rz,np.sin(T*f(.05))*f(.2))
    t=np.full_like(px,f(.1)); ac=np.zeros_like(px)
    for i in range(99):
        x=rx*t; y=f(-.2)+ry*t; z=T*f(4.)+rz*t
        x=np.mod(x-2,f(4.))-2; y=np.mod(y-2,f(4.))-2; z=np.mod(z-2,f(4.))-2
        g=T-f(i)*f(0.01)
        d=box_set(x,y,z,g); d=np.maximum(np.abs(d),f(.01))
        ac+=np.exp(-d*f(23.)); t+=d*f(.55)
    c=ac*f(.02)
    col=np.stack([c, c+f(.2)*abs(np.sin(T)), c+f(.5)+np.sin(T)*f(.2)],-1)
    img=(np.clip(col,0,1)*255+.5).astype(np.uint8)[::-1]
    return img
T=float(sys.argv[1]); W=int(sys.argv[2]); H=int(sys.argv[3])
img=render(W,H,T); Image.fromarray(img).save(sys.argv[4])
print('mean rgb', img.reshape(-1,3).mean(0))
