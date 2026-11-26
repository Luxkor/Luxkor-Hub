#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define GH_USER "Luxkor"
#define TOPIC "\"luxkor-app\""
#define MAXA 64
#define WM_UI (WM_APP+1)
#define SBW 236
#define HDR 64
#define FOOT 56
#define PAD 24
#define GAP 16
#define CARDH 172
#define MINCARD 300
#define S(x) MulDiv(x,dpi,96)
/* paleta (tomada de LuxkorAccountManager) */
#define K_SIDE 0x08080b
#define K_BG   0x0f0f14
#define K_PAN  0x1a1a22
#define K_PAN2 0x23232c
#define K_LINE 0x2e2e3a
#define K_INK  0xf4f4f7
#define K_MUT  0x9696a6
#define K_FNT  0x62626f
#define K_ACC  0xff3347
#define K_OK   0x3ddc97
#define K_WARN 0xffc65c
#define K_DNG  0xff6b7d
#define A_(h) (0xFF000000u|(UINT32)(h))
#define CO(h) RGB(((h)>>16)&255,((h)>>8)&255,(h)&255)

typedef struct{char name[64];WCHAR wn[64];WCHAR dn[64];WCHAR desc[200];char tag[40];char url[512];char inst[40];volatile int busy,pct,cancel;volatile DWORD done,total;}App;
static App A[MAXA];
static volatile int N,loading,neterr;
static WCHAR status[160];
static HWND hw;
static int dpi=96,W,H,drag,hvi=-1,hva,hvc=-1,hvn=-1,nav,vis[MAXA],nv;
static float sy,ty,navy;
static HFONT fT,fL,fN,fS,fB,fI;
static HBRUSH bBg,bSd,bLn;
static void*G;

static void setstatus(const WCHAR*s){wcsncpy(status,s,159);status[159]=0;PostMessage(hw,WM_UI,0,0);}
static void W16(const char*s,WCHAR*o,int n){MultiByteToWideChar(CP_UTF8,0,s,-1,o,n);}

static int http(const char*url,HANDLE file,int idx,char**out){
  WCHAR wu[700],host[256],path[1300],ex[600];
  W16(url,wu,700);
  URL_COMPONENTS uc;memset(&uc,0,sizeof uc);uc.dwStructSize=sizeof uc;
  uc.lpszHostName=host;uc.dwHostNameLength=256;uc.lpszUrlPath=path;uc.dwUrlPathLength=600;uc.lpszExtraInfo=ex;uc.dwExtraInfoLength=600;
  if(!WinHttpCrackUrl(wu,0,0,&uc))return 0;
  wcscat(path,ex);
  HINTERNET s=WinHttpOpen(L"LuxkorHub/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
  if(!s)return 0;
  HINTERNET c=WinHttpConnect(s,host,uc.nPort,0);
  HINTERNET r=c?WinHttpOpenRequest(c,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,uc.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0):NULL;
  int ok=0;
  if(r){
    if(!file)WinHttpAddRequestHeaders(r,L"Accept: application/vnd.github+json",(DWORD)-1,WINHTTP_ADDREQ_FLAG_ADD);
    if(WinHttpSendRequest(r,WINHTTP_NO_ADDITIONAL_HEADERS,0,NULL,0,0,0)&&WinHttpReceiveResponse(r,NULL)){
      DWORD code=0,sz=sizeof code;
      WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&code,&sz,NULL);
      if(code==200){
        DWORD total=0;sz=sizeof total;
        WinHttpQueryHeaders(r,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,NULL,&total,&sz,NULL);
        static __thread char buf[32768];
        size_t cap=0,len=0;char*m=NULL;DWORD n=0,done=0;BOOL rd;ok=1;
        while((rd=WinHttpReadData(r,buf,sizeof buf,&n))&&n){
          if(file&&idx>=0&&A[idx].cancel){ok=0;break;}
          if(file){DWORD w;WriteFile(file,buf,n,&w,NULL);done+=n;
            if(idx>=0){A[idx].done=done;A[idx].total=total;if(total){int p=(int)(done*100.0/total);if(p!=A[idx].pct){A[idx].pct=p;PostMessage(hw,WM_UI,0,0);}}}}
          else{if(len+n+1>cap){cap=(len+n+1)*2;m=realloc(m,cap);}memcpy(m+len,buf,n);len+=n;}
        }
        if(!rd)ok=0;if(file&&total&&done!=total)ok=0;
        if(!file){if(m)m[len]=0;*out=m;}
      }
    }
  }
  if(r)WinHttpCloseHandle(r);if(c)WinHttpCloseHandle(c);WinHttpCloseHandle(s);
  return ok;
}

static int jstr(const char*p,const char*lim,const char*key,char*out,int max){
  const char*k=strstr(p,key);if(!k||(lim&&k>=lim))return 0;
  k+=strlen(key);while(*k==' ')k++;
  if(*k!='"')return 0;k++;
  int i=0;
  while(*k&&*k!='"'&&i<max-1){
    if(*k=='\\'){k++;if(*k=='u'){k+=5;out[i++]='?';continue;}out[i++]=(*k=='n'||*k=='r'||*k=='t')?' ':*k;k++;continue;}
    out[i++]=*k++;
  }
  out[i]=0;return 1;
}

static void appdir(const char*name,WCHAR*out){
  WCHAR b[MAX_PATH],w[64];GetEnvironmentVariableW(L"LOCALAPPDATA",b,MAX_PATH);W16(name,w,64);
  swprintf(out,MAX_PATH,L"%ls\\LuxkorHub\\apps\\%ls",b,w);
}
static void mkdirs(WCHAR*p){for(WCHAR*c=p+3;*c;c++)if(*c==L'\\'){*c=0;CreateDirectoryW(p,0);*c=L'\\';}CreateDirectoryW(p,0);}
static void fpath(const App*a,const WCHAR*file,WCHAR*o){WCHAR d[MAX_PATH];appdir(a->name,d);swprintf(o,MAX_PATH,L"%ls\\%ls",d,file);}

static DWORD WINAPI loader(LPVOID v){
  loading=1;neterr=0;N=0;setstatus(L"Buscando apps…");
  char*js=0;
  if(!http("https://api.github.com/users/" GH_USER "/repos?per_page=100",0,-1,&js)||!js){
    free(js);loading=0;neterr=1;setstatus(L"No se pudo conectar con GitHub (¿sin internet o límite alcanzado?)");return 0;}
  const char*FN="\"full_name\":\"" GH_USER "/";const char*ANY="\"full_name\":\"";
  char*p=js;
  while(N<MAXA&&(p=strstr(p,FN))){
    char*nx=strstr(p+10,ANY);char*t=strstr(p,TOPIC);
    char nm[64];const char*q=p+strlen(FN);int i=0;while(*q&&*q!='"'&&i<63)nm[i++]=*q++;nm[i]=0;
    char*cur=p;p+=10;
    if(!t||(nx&&t>=nx)||!nm[0]||nm[0]=='.')continue;
    int okn=1;for(i=0;nm[i];i++)if(!(isalnum((unsigned char)nm[i])||nm[i]=='-'||nm[i]=='_'||nm[i]=='.'))okn=0;
    if(!okn)continue;
    char u[200];snprintf(u,200,"https://api.github.com/repos/" GH_USER "/%s/releases/latest",nm);
    char*rel=0;if(!http(u,0,-1,&rel)||!rel){free(rel);continue;}
    App*a=&A[N];memset(a,0,sizeof *a);strcpy(a->name,nm);W16(nm,a->wn,64);wcscpy(a->dn,a->wn);for(WCHAR*z=a->dn;*z;z++)if(*z==L'-'||*z==L'_')*z=L' ';
    char d[200];if(jstr(cur,nx,"\"description\":",d,200))W16(d,a->desc,200);
    int ok=jstr(rel,0,"\"tag_name\":",a->tag,40),f=0;
    char*q2=rel;
    while(ok&&(q2=strstr(q2,"\"browser_download_url\":"))){
      if(jstr(q2,0,"\"browser_download_url\":",a->url,512)){size_t L=strlen(a->url);if(L>4&&!_stricmp(a->url+L-4,".exe")){f=1;break;}}
      q2+=10;
    }
    free(rel);
    if(ok&&f){
      WCHAR vf[MAX_PATH];fpath(a,L"version.txt",vf);
      HANDLE h=CreateFileW(vf,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
      if(h!=INVALID_HANDLE_VALUE){DWORD r=0;ReadFile(h,a->inst,39,&r,0);a->inst[r]=0;CloseHandle(h);}
      if(a->inst[0]){WCHAR ex[80],ef[MAX_PATH];swprintf(ex,80,L"%ls.exe",a->wn);fpath(a,ex,ef);if(GetFileAttributesW(ef)==INVALID_FILE_ATTRIBUTES)a->inst[0]=0;}
      N++;PostMessage(hw,WM_UI,0,0);
    }
  }
  free(js);loading=0;
  setstatus(N?L"":L"No hay apps con el topic luxkor-app y un Release con .exe");
  return 0;
}

static DWORD WINAPI installer(LPVOID v){
  int i=(int)(INT_PTR)v;App*a=&A[i];WCHAR d[MAX_PATH],t[MAX_PATH],e[MAX_PATH],o[MAX_PATH],vf[MAX_PATH],ex[80],msg[160];
  appdir(a->name,d);mkdirs(d);a->done=a->total=0;
  fpath(a,L"tmp.part",t);swprintf(ex,80,L"%ls.exe",a->wn);fpath(a,ex,e);swprintf(ex,80,L"%ls.exe.old",a->wn);fpath(a,ex,o);fpath(a,L"version.txt",vf);
  DeleteFileW(o);
  HANDLE f=CreateFileW(t,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  int ok=0;if(f!=INVALID_HANDLE_VALUE){ok=http(a->url,f,i,NULL);CloseHandle(f);}
  const WCHAR*err=NULL;
  if(a->cancel)err=L"Descarga cancelada";
  else if(!ok)err=L"Error de descarga: revisa tu conexión";
  else{
    char mz[2]={0,0};DWORD r=0;HANDLE h=CreateFileW(t,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if(h!=INVALID_HANDLE_VALUE){ReadFile(h,mz,2,&r,0);CloseHandle(h);}
    if(r!=2||mz[0]!='M'||mz[1]!='Z')err=L"El archivo descargado no es un .exe válido";
  }
  if(!err){
    int mv=MoveFileExW(t,e,MOVEFILE_REPLACE_EXISTING);
    if(!mv&&MoveFileW(e,o)){mv=MoveFileW(t,e);if(!mv)MoveFileW(o,e);}
    if(!mv)err=L"No se pudo reemplazar el archivo";
  }
  if(err){DeleteFileW(t);setstatus(err);}
  else{
    HANDLE h=CreateFileW(vf,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    DWORD w;if(h!=INVALID_HANDLE_VALUE){WriteFile(h,a->tag,(DWORD)strlen(a->tag),&w,0);CloseHandle(h);}
    int was=a->inst[0];strcpy(a->inst,a->tag);
    swprintf(msg,160,L"%ls %ls correctamente",a->dn,was?L"actualizada":L"instalada");setstatus(msg);
  }
  a->busy=0;a->cancel=0;PostMessage(hw,WM_UI,0,0);return 0;
}

/* ---------- GDI+ (API plana, sin cabeceras C++) ---------- */
typedef void*GP;
typedef struct{UINT32 v;void*cb;BOOL a,b;}GSI;
typedef struct{int x,y,w,h;}GRI;
int WINAPI GdiplusStartup(ULONG_PTR*,const GSI*,void*);
int WINAPI GdipCreateFromHDC(HDC,GP*);
int WINAPI GdipDeleteGraphics(GP);
int WINAPI GdipSetSmoothingMode(GP,int);
int WINAPI GdipFlush(GP,int);
int WINAPI GdipCreateSolidFill(UINT32,GP*);
int WINAPI GdipDeleteBrush(GP);
int WINAPI GdipCreatePath(int,GP*);
int WINAPI GdipDeletePath(GP);
int WINAPI GdipAddPathArc(GP,float,float,float,float,float,float);
int WINAPI GdipClosePathFigure(GP);
int WINAPI GdipFillPath(GP,GP,GP);
int WINAPI GdipDrawPath(GP,GP,GP);
int WINAPI GdipCreatePen1(UINT32,float,int,GP*);
int WINAPI GdipDeletePen(GP);
int WINAPI GdipFillEllipse(GP,GP,float,float,float,float);
int WINAPI GdipDrawLine(GP,GP,float,float,float,float);
int WINAPI GdipCreateBitmapFromStream(void*,GP*);
int WINAPI GdipDrawImageRectI(GP,GP,int,int,int,int);
int WINAPI GdipSetInterpolationMode(GP,int);
static GP logo;
static void loadlogo(HINSTANCE hi){
  HRSRC r=FindResourceW(hi,MAKEINTRESOURCEW(2),RT_RCDATA);if(!r)return;
  void*p=LockResource(LoadResource(hi,r));DWORD n=SizeofResource(hi,r);
  void*st=SHCreateMemStream((const BYTE*)p,n);if(st)GdipCreateBitmapFromStream(st,&logo);
}
int WINAPI GdipCreateLineBrushFromRectI(const GRI*,UINT32,UINT32,int,int,GP*);

static GP rrp(float x,float y,float w,float h,float r){
  GP p;if(r*2>h)r=h/2;if(r*2>w)r=w/2;float d=r*2;GdipCreatePath(0,&p);
  GdipAddPathArc(p,x,y,d,d,180,90);GdipAddPathArc(p,x+w-d,y,d,d,270,90);
  GdipAddPathArc(p,x+w-d,y+h-d,d,d,0,90);GdipAddPathArc(p,x,y+h-d,d,d,90,90);
  GdipClosePathFigure(p);return p;
}
static void box(RECT r,int rad,UINT32 fill,UINT32 line){
  GP b,pn,p=rrp(r.left+.5f,r.top+.5f,(float)(r.right-r.left-1),(float)(r.bottom-r.top-1),(float)rad);
  if(fill){GdipCreateSolidFill(A_(fill),&b);GdipFillPath(G,b,p);GdipDeleteBrush(b);}
  if(line){GdipCreatePen1(A_(line),1,2,&pn);GdipDrawPath(G,pn,p);GdipDeletePen(pn);}
  GdipDeletePath(p);
}
static void gbox(RECT r,int rad,UINT32 c1,UINT32 c2){
  GRI g={r.left,r.top,r.right-r.left,r.bottom-r.top};GP b,p;
  if(g.w<2||g.h<2)return;
  p=rrp((float)r.left,(float)r.top,(float)g.w,(float)g.h,(float)rad);
  GdipCreateLineBrushFromRectI(&g,A_(c1),A_(c2),1,0,&b);GdipFillPath(G,b,p);GdipDeleteBrush(b);GdipDeletePath(p);
}
static void dotc(float x,float y,float d,UINT32 c){GP b;GdipCreateSolidFill(A_(c),&b);GdipFillEllipse(G,b,x,y,d,d);GdipDeleteBrush(b);}
static void T(HDC dc,HFONT f,COLORREF c,const WCHAR*s,int x,int y,int w,int h,UINT fl){
  RECT r={x,y,x+w,y+h};GdipFlush(G,1);SelectObject(dc,f);SetTextColor(dc,c);
  DrawTextW(dc,s,-1,&r,fl|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);
}

/* ---------- datos de la vista ---------- */
static int isupd(const App*a){return a->inst[0]&&strcmp(a->inst,a->tag);}
static int cnt(int m){int n=0;for(int i=0;i<N;i++)if(m==0||(m==1&&A[i].inst[0])||(m==2&&isupd(&A[i])))n++;return n;}
static void rebuild(void){
  nv=0;int n=N;
  for(int i=0;i<n;i++){const App*a=&A[i];if(nav==0||(nav==1&&a->inst[0])||(nav==2&&isupd(a)))vis[nv++]=i;}
}
static const WCHAR*NAVL[3]={L"Aplicaciones",L"Instaladas",L"Actualizaciones"};
static const WCHAR*NAVS[3]={L"Instala y actualiza tus apps sin pasar por GitHub",L"Apps instaladas en este equipo",L"Apps con una versión nueva disponible"};

static int cols(void){int w=W-S(SBW)-2*S(PAD);int c=(w+S(GAP))/(S(MINCARD)+S(GAP));return c<1?1:c;}
static int cardw(void){int c=cols();return (W-S(SBW)-2*S(PAD)-(c-1)*S(GAP))/c;}
static RECT cardrc(int k){
  int c=cols(),cw=cardw(),row=k/c,col=k%c;RECT r;
  r.left=S(SBW)+S(PAD)+col*(cw+S(GAP));r.top=S(HDR)+S(PAD)+row*(S(CARDH)+S(GAP))-(int)sy;
  r.right=r.left+cw;r.bottom=r.top+S(CARDH);return r;
}
static int content_h(void){int c=cols();return S(PAD)+((nv+c-1)/c)*(S(CARDH)+S(GAP));}
static int view_h(void){return H-S(HDR)-S(FOOT);}
static float maxs(void){float m=(float)(content_h()-view_h());return m>0?m:0;}
static void clampy(void){float m=maxs();if(ty<0)ty=0;if(ty>m)ty=m;if(sy<0)sy=0;if(sy>m)sy=m;}
static int thumbh(void){int vh=view_h(),c=content_h();int t=(int)((float)vh*vh/c);return t<S(40)?S(40):t;}
static RECT refr(void){RECT r;r.right=W-S(PAD);r.left=r.right-S(116);r.top=(S(HDR)-S(34))/2;r.bottom=r.top+S(34);return r;}
static RECT footbtn(void){RECT r;r.right=W-S(PAD);r.left=r.right-S(200);r.top=H-S(FOOT)+(S(FOOT)-S(36))/2;r.bottom=r.top+S(36);return r;}
static RECT navrc(int k){RECT r={S(12),S(104)+k*S(46),S(SBW)-S(12),S(104)+k*S(46)+S(40)};return r;}
static int navhit(int x,int y){POINT p={x,y};for(int k=0;k<3;k++){RECT r=navrc(k);if(PtInRect(&r,p))return k;}return -1;}

static int btns(int i,RECT c,int*a,RECT*r){
  App*p=&A[i];int n=0;
  if(p->busy){a[0]=5;r[0].right=c.right-S(16);r[0].left=r[0].right-S(40);r[0].top=c.bottom-S(52);r[0].bottom=r[0].top+S(36);return 1;}
  if(!p->inst[0])a[n++]=1;else{if(isupd(p))a[n++]=1;a[n++]=3;a[n++]=4;}
  int x0=c.left+S(16),x1=c.right-S(16),g=S(8),fixed=0,y=c.bottom-S(16)-S(36);
  for(int k=1;k<n;k++)fixed+=(a[k]==3?S(76):S(40))+g;
  int x=x0;
  for(int k=0;k<n;k++){
    int w=k==0?(x1-x0)-fixed:(a[k]==3?S(76):S(40));
    r[k].left=x;r[k].right=x+w;r[k].top=y;r[k].bottom=y+S(36);x+=w+g;
  }
  return n;
}
static int hit(int x,int y,int*act){
  POINT pt={x,y};*act=0;
  {RECT r=refr();if(PtInRect(&r,pt)){*act=9;return -2;}}
  {RECT r=footbtn();if(PtInRect(&r,pt)){*act=8;return -3;}}
  if(x>=S(SBW)&&y>=S(HDR)&&y<H-S(FOOT))
    for(int k=0;k<nv;k++){int i=vis[k],a[3];RECT r[3],c=cardrc(k);int n=btns(i,c,a,r);
      for(int j=0;j<n;j++)if(PtInRect(&r[j],pt)){*act=a[j];return i;}}
  return -1;
}
static void doact(int i,int a){
  App*p=&A[i];WCHAR f[MAX_PATH],d[MAX_PATH],ex[80];
  if(a==1){if(p->busy)return;p->busy=1;p->pct=0;p->cancel=0;CreateThread(0,0,installer,(LPVOID)(INT_PTR)i,0,0);}
  else if(a==5){p->cancel=1;}
  else if(a==3){appdir(p->name,d);swprintf(ex,80,L"%ls.exe",p->wn);fpath(p,ex,f);ShellExecuteW(hw,L"open",f,NULL,d,SW_SHOWNORMAL);}
  else if(a==4){
    WCHAR q[120];swprintf(q,120,L"¿Desinstalar %ls?",p->dn);
    if(MessageBoxW(hw,q,L"Luxkor Hub",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    appdir(p->name,d);swprintf(ex,80,L"%ls.exe",p->wn);fpath(p,ex,f);
    if(!DeleteFileW(f)&&GetLastError()!=ERROR_FILE_NOT_FOUND){setstatus(L"Cierra la app antes de desinstalarla");return;}
    swprintf(ex,80,L"%ls.exe.old",p->wn);fpath(p,ex,f);DeleteFileW(f);
    fpath(p,L"version.txt",f);DeleteFileW(f);fpath(p,L"tmp.part",f);DeleteFileW(f);RemoveDirectoryW(d);
    p->inst[0]=0;swprintf(f,MAX_PATH,L"%ls desinstalada",p->dn);setstatus(f);
  }
}

/* ---------- dibujo ---------- */
static int pwid(HDC dc,const WCHAR*t){SIZE z;SelectObject(dc,fS);GetTextExtentPoint32W(dc,t,(int)wcslen(t),&z);return S(34)+z.cx;}
static int pill(HDC dc,int x,int y,const WCHAR*t,UINT32 fill,UINT32 line,UINT32 dc2,COLORREF tc){
  int w=pwid(dc,t);RECT r={x,y,x+w,y+S(22)};box(r,S(11),fill,line);
  dotc((float)(x+S(12)),(float)(y+S(7)),(float)S(8),dc2);T(dc,fS,tc,t,x+S(26),y,w-S(26),S(22),DT_VCENTER);return w;
}
static void btn(HDC dc,RECT r,const WCHAR*lb,int primary,int hov){
  if(primary)gbox(r,S(10),hov?0xff5566:K_ACC,hov?0xe8283f:0xd91f38);
  else box(r,S(10),hov?0x2a2a35:0x15151b,hov?0x4a4a5a:K_LINE);
  T(dc,fB,CO(K_INK),lb,r.left,r.top,r.right-r.left,r.bottom-r.top,DT_CENTER|DT_VCENTER);
}

static void paint(HDC dc){
  GdipCreateFromHDC(dc,&G);GdipSetSmoothingMode(G,4);
  RECT all={0,0,W,H};FillRect(dc,&all,bBg);SetBkMode(dc,TRANSPARENT);
  int hd=S(HDR),ft=H-S(FOOT),sb=S(SBW);
  if(!nv){
    const WCHAR*m=loading?L"Buscando apps…":nav==0?L"Aún no hay apps publicadas":nav==1?L"No tienes apps instaladas":L"Todo está al día";
    T(dc,fN,CO(K_INK),m,sb,hd+S(90),W-sb,S(28),DT_CENTER);
    if(!loading&&nav==0)T(dc,fS,CO(K_MUT),L"Añade el topic luxkor-app a tu repo y publica un Release con el .exe",sb,hd+S(122),W-sb,S(24),DT_CENTER);
  }
  for(int k=0;k<nv;k++){
    int i=vis[k];App*a=&A[i];RECT c=cardrc(k);if(c.bottom<hd||c.top>ft)continue;
    int upd=isupd(a),hv=(hvc==i);
    box(c,S(16),hv?K_PAN2:K_PAN,hv?0x3a3a48:K_LINE);
    RECT ic={c.left+S(16),c.top+S(16),c.left+S(60),c.top+S(60)};gbox(ic,S(12),K_ACC,0xc4162b);
    WCHAR l[2]={towupper(a->wn[0]),0};T(dc,fI,CO(K_INK),l,ic.left,ic.top,S(44),S(44),DT_CENTER|DT_VCENTER);
    int tx=ic.right+S(12);
    T(dc,fN,CO(K_INK),a->dn,tx,c.top+S(14),c.right-S(16)-tx,S(22),0);
    const WCHAR*pt;UINT32 pf,pb,pd;COLORREF pc;
    if(a->busy){pt=L"Descargando";pf=0x1c1318;pb=0x4a1f2a;pd=K_ACC;pc=CO(K_DNG);}
    else if(upd){pt=L"Actualización";pf=0x211a0e;pb=0x4a3a14;pd=K_WARN;pc=CO(K_WARN);}
    else if(a->inst[0]){pt=L"Instalada";pf=0x0e211a;pb=0x1a4a38;pd=K_OK;pc=CO(K_OK);}
    else{pt=L"No instalada";pf=0x15151b;pb=K_LINE;pd=K_FNT;pc=CO(K_MUT);}
    int pw=pill(dc,tx,c.top+S(40),pt,pf,pb,pd,pc);
    WCHAR vs[100],v1[40],v2[40];W16(a->tag,v2,40);
    if(a->inst[0]){W16(a->inst,v1,40);if(upd)swprintf(vs,100,L"%ls → %ls",v1,v2);else wcscpy(vs,v1);}else wcscpy(vs,v2);
    {int vx=tx+pw+S(8);T(dc,fS,CO(K_FNT),vs,vx,c.top+S(40),c.right-S(16)-vx,S(22),DT_RIGHT|DT_VCENTER);}
    {RECT dr={c.left+S(16),c.top+S(74),c.right-S(16),c.top+S(74)+S(36)};GdipFlush(G,1);SelectObject(dc,fS);SetTextColor(dc,CO(K_MUT));
     DrawTextW(dc,a->desc[0]?a->desc:L"Sin descripción",-1,&dr,DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);}
    if(a->busy){
      int rt=c.bottom-S(52),x0=c.left+S(16),x1=c.right-S(16)-S(48);
      RECT tr={x0,rt+S(26),x1,rt+S(34)};box(tr,S(4),0x0f0f14,0);
      RECT fr=tr;fr.right=tr.left+(tr.right-tr.left)*a->pct/100;if(fr.right-fr.left>S(8))gbox(fr,S(4),K_ACC,0xd91f38);
      WCHAR pp[64];if(a->total)swprintf(pp,64,L"%.1f / %.1f MB · %d%%",a->done/1048576.0,a->total/1048576.0,a->pct);else wcscpy(pp,L"Descargando…");
      T(dc,fS,CO(K_MUT),pp,x0,rt+S(2),x1-x0,S(20),0);
      {int ac[3];RECT br[3];btns(i,c,ac,br);int h2=(hvi==i&&hva==5);
       box(br[0],S(10),h2?0x1c1318:0x15151b,h2?0x4a1f2a:K_LINE);
       float cx=(float)(br[0].left+br[0].right)/2,cy=(float)(br[0].top+br[0].bottom)/2,d=(float)S(5);GP pn;
       GdipCreatePen1(A_(h2?K_DNG:K_MUT),(float)S(2),2,&pn);
       GdipDrawLine(G,pn,cx-d,cy-d,cx+d,cy+d);GdipDrawLine(G,pn,cx-d,cy+d,cx+d,cy-d);GdipDeletePen(pn);}
    }else{
      int ac[3];RECT br[3];int n=btns(i,c,ac,br);
      for(int j=0;j<n;j++){
        int h2=(hvi==i&&hva==ac[j]);
        if(ac[j]==4){
          box(br[j],S(10),h2?0x1c1318:0x15151b,h2?0x4a1f2a:K_LINE);
          float cx=(float)(br[j].left+br[j].right)/2,cy=(float)(br[j].top+br[j].bottom)/2,d=(float)S(5);GP pn;
          GdipCreatePen1(A_(h2?K_DNG:K_MUT),(float)S(2),2,&pn);
          GdipDrawLine(G,pn,cx-d,cy-d,cx+d,cy+d);GdipDrawLine(G,pn,cx-d,cy+d,cx+d,cy-d);GdipDeletePen(pn);
        }else btn(dc,br[j],ac[j]==1?(a->inst[0]?L"Actualizar":L"Instalar"):L"Abrir",j==0,h2);
      }
    }
  }
  GdipFlush(G,1);
  /* barra lateral */
  {RECT sr={0,0,sb,H};FillRect(dc,&sr,bSd);RECT ln={sb-1,0,sb,H};FillRect(dc,&ln,bLn);}
  {RECT lg={S(18),S(20),S(66),S(68)};
   if(logo){GdipSetInterpolationMode(G,7);GdipDrawImageRectI(G,logo,lg.left,lg.top,S(48),S(48));}
   else{gbox(lg,S(14),K_ACC,0xc4162b);T(dc,fL,CO(K_INK),L"L",lg.left,lg.top,S(48),S(48),DT_CENTER|DT_VCENTER);}
   T(dc,fL,CO(K_INK),L"Luxkor",S(80),S(22),S(140),S(26),0);
   T(dc,fS,CO(K_MUT),L"Hub de aplicaciones",S(80),S(48),S(150),S(20),0);}
  for(int k=0;k<3;k++){RECT r=navrc(k);if(hvn==k&&k!=nav)box(r,S(10),0x15151b,0);}
  {RECT ar=navrc(0);OffsetRect(&ar,0,(int)navy);box(ar,S(10),0x1c1318,0x3a1a22);
   RECT bar={ar.left+S(6),ar.top+S(10),ar.left+S(9),ar.bottom-S(10)};gbox(bar,S(2),K_ACC,0xd91f38);}
  for(int k=0;k<3;k++){
    RECT r=navrc(k);WCHAR n[16];swprintf(n,16,L"%d",cnt(k));
    T(dc,fB,CO((k==nav||hvn==k)?K_INK:K_MUT),NAVL[k],r.left+S(22),r.top,r.right-r.left-S(60),S(40),DT_VCENTER);
    T(dc,fS,CO(k==2&&cnt(2)>0?K_WARN:K_FNT),n,r.right-S(46),r.top,S(34),S(40),DT_VCENTER|DT_RIGHT);
  }
  T(dc,fS,CO(K_FNT),L"Cuenta · " L"Luxkor",S(20),H-S(40),S(190),S(22),0);
  /* cabecera */
  {RECT hr={sb,0,W,hd};FillRect(dc,&hr,bBg);RECT ln={sb,hd-1,W,hd};FillRect(dc,&ln,bLn);}
  T(dc,fT,CO(K_INK),NAVL[nav],sb+S(PAD),S(10),S(360),S(28),0);
  T(dc,fS,CO(K_MUT),NAVS[nav],sb+S(PAD),S(38),S(420),S(20),0);
  {RECT rf=refr();btn(dc,rf,loading?L"Buscando…":L"Actualizar",0,hvi==-2&&!loading);
   const WCHAR*pt=loading?L"Buscando…":neterr?L"Sin conexión":L"Conectado";
   UINT32 pf=loading?0x211a0e:neterr?0x1c1318:0x0e211a,pb=loading?0x4a3a14:neterr?0x4a1f2a:0x1a4a38,pd=loading?K_WARN:neterr?K_DNG:K_OK;
   COLORREF pc=CO(pd);int w=pwid(dc,pt);pill(dc,rf.left-S(12)-w,(hd-S(22))/2,pt,pf,pb,pd,pc);}
  /* pie */
  {RECT fr={sb,ft,W,H};FillRect(dc,&fr,bBg);RECT ln={sb,ft,W,ft+1};FillRect(dc,&ln,bLn);
   WCHAR t[120];int nu=cnt(2);swprintf(t,120,L"%d apps · %d instaladas",N,cnt(1));
   T(dc,fS,CO(K_MUT),t,sb+S(PAD),ft+1,S(220),H-ft-1,DT_VCENTER);
   T(dc,fS,CO(K_FNT),status,sb+S(PAD)+S(230),ft+1,W-sb-S(PAD)*2-S(230)-S(216),H-ft-1,DT_VCENTER|DT_RIGHT);
   RECT fb=footbtn();WCHAR bt[60];if(nu)swprintf(bt,60,L"Actualizar todo (%d)",nu);else wcscpy(bt,L"Todo al día");
   if(nu)btn(dc,fb,bt,1,hvi==-3);else{box(fb,S(10),0x15151b,K_LINE);T(dc,fB,CO(K_FNT),bt,fb.left,fb.top,fb.right-fb.left,fb.bottom-fb.top,DT_CENTER|DT_VCENTER);}}
  /* scroll */
  float m=maxs();
  if(m>0){int vh=view_h(),th=thumbh(),t0=hd+(int)(sy/m*(vh-th));RECT sbr={W-S(10),t0,W-S(4),t0+th};box(sbr,S(3),drag?K_ACC:0x3a3a48,0);}
  GdipFlush(G,1);GdipDeleteGraphics(G);G=0;
}

static void scrollto(int y){
  float m=maxs();int vh=view_h(),th=thumbh();if(vh-th<=0)return;
  float r=(float)(y-S(HDR)-th/2)/(vh-th);sy=ty=r*m;clampy();InvalidateRect(hw,0,FALSE);
}

static LRESULT CALLBACK WP(HWND h,UINT m,WPARAM w,LPARAM l){
  static HDC md;static HBITMAP bm,ob;static int bw,bh;
  switch(m){
  case WM_CREATE:{BOOL d=1;DwmSetWindowAttribute(h,20,&d,sizeof d);COLORREF c=RGB(15,15,20);DwmSetWindowAttribute(h,35,&c,sizeof c);return 0;}
  case WM_SIZE:W=LOWORD(l);H=HIWORD(l);clampy();InvalidateRect(h,0,FALSE);return 0;
  case WM_GETMINMAXINFO:{MINMAXINFO*mi=(MINMAXINFO*)l;mi->ptMinTrackSize.x=S(800);mi->ptMinTrackSize.y=S(520);return 0;}
  case WM_ERASEBKGND:return 1;
  case WM_PAINT:{
    PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
    if(!md)md=CreateCompatibleDC(dc);
    if(bw<W||bh<H){if(bm){SelectObject(md,ob);DeleteObject(bm);}bw=W+128;bh=H+128;bm=CreateCompatibleBitmap(dc,bw,bh);ob=SelectObject(md,bm);}
    paint(md);
    BitBlt(dc,ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right-ps.rcPaint.left,ps.rcPaint.bottom-ps.rcPaint.top,md,ps.rcPaint.left,ps.rcPaint.top,SRCCOPY);
    EndPaint(h,&ps);return 0;}
  case WM_UI:rebuild();clampy();InvalidateRect(h,0,FALSE);return 0;
  case WM_MOUSEWHEEL:ty-=(float)GET_WHEEL_DELTA_WPARAM(w)/120*S(90);clampy();SetTimer(h,1,15,0);return 0;
  case WM_TIMER:
    if(w==2){float t=(float)(nav*S(46));navy+=(t-navy)*0.25f;if(fabsf(t-navy)<0.5f){navy=t;KillTimer(h,2);}}
    else{sy+=(ty-sy)*0.3f;if(fabsf(ty-sy)<0.6f){sy=ty;KillTimer(h,1);}}
    InvalidateRect(h,0,FALSE);return 0;
  case WM_LBUTTONDOWN:{
    int x=(short)LOWORD(l),y=(short)HIWORD(l),a,i=hit(x,y,&a),n=navhit(x,y);
    if(n>=0){if(n!=nav){nav=n;rebuild();sy=ty=0;SetTimer(h,2,15,0);}}
    else if(i==-2){if(!loading){loading=1;CreateThread(0,0,loader,0,0,0);}}
    else if(i==-3){for(int k=0;k<N;k++)if(isupd(&A[k])&&!A[k].busy)doact(k,1);}
    else if(i>=0)doact(i,a);
    else if(x>=W-S(16)&&y>S(HDR)&&y<H-S(FOOT)&&maxs()>0){drag=1;SetCapture(h);scrollto(y);}
    InvalidateRect(h,0,FALSE);return 0;}
  case WM_LBUTTONUP:if(drag){drag=0;ReleaseCapture();InvalidateRect(h,0,FALSE);}return 0;
  case WM_MOUSEMOVE:{
    int x=(short)LOWORD(l),y=(short)HIWORD(l),a,c=-1;
    if(drag){scrollto(y);return 0;}
    TRACKMOUSEEVENT te={sizeof te,TME_LEAVE,h,0};TrackMouseEvent(&te);
    int i=hit(x,y,&a),n=navhit(x,y);
    if(y>=S(HDR)&&y<H-S(FOOT)){POINT p={x,y};for(int k=0;k<nv;k++){RECT r=cardrc(k);if(PtInRect(&r,p))c=vis[k];}}
    if(i!=hvi||a!=hva||c!=hvc||n!=hvn){hvi=i;hva=a;hvc=c;hvn=n;InvalidateRect(h,0,FALSE);}return 0;}
  case WM_MOUSELEAVE:hvc=-1;hvi=-1;hvn=-1;hva=0;InvalidateRect(h,0,FALSE);return 0;
  case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT){SetCursor(LoadCursor(0,(hvi!=-1||hvn!=-1)?IDC_HAND:IDC_ARROW));return TRUE;}break;
  case WM_DESTROY:PostQuitMessage(0);return 0;
  }
  return DefWindowProc(h,m,w,l);
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cl,int sh){
  HDC sd=GetDC(0);dpi=GetDeviceCaps(sd,LOGPIXELSX);ReleaseDC(0,sd);
  ULONG_PTR tok;GSI gi={1,0,0,0};GdiplusStartup(&tok,&gi,0);loadlogo(hi);
#define F(px,wt) CreateFontW(-S(px),0,0,0,wt,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI")
  fT=F(22,FW_BOLD);fL=F(19,FW_BOLD);fN=F(15,FW_SEMIBOLD);fS=F(13,FW_NORMAL);fB=F(13,FW_SEMIBOLD);fI=F(22,FW_BOLD);
  bBg=CreateSolidBrush(CO(K_BG));bSd=CreateSolidBrush(CO(K_SIDE));bLn=CreateSolidBrush(CO(K_LINE));
  WNDCLASSW wc;memset(&wc,0,sizeof wc);wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WP;wc.hInstance=hi;
  wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hIcon=LoadIcon(hi,MAKEINTRESOURCE(1));wc.lpszClassName=L"LuxkorHub";RegisterClassW(&wc);
  hw=CreateWindowExW(0,L"LuxkorHub",L"Luxkor Hub",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,S(1040),S(660),0,0,hi,0);
  loading=1;CreateThread(0,0,loader,0,0,0);
  MSG ms;while(GetMessage(&ms,0,0,0)){TranslateMessage(&ms);DispatchMessage(&ms);}
  return 0;
}
