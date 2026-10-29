#include "ui_utf8.h"
#include <string.h>
static size_t ancho(const unsigned char *s, size_t n) {
    if (!n) return 0;
    unsigned char c=s[0];
    if (c<0x80) return 1;
    size_t k=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
    if (!k || n<k) return 0;
    for(size_t i=1;i<k;++i) if ((s[i]&0xc0)!=0x80) return 0;
    if ((c==0xe0 && s[1]<0xa0) || (c==0xed && s[1]>=0xa0) ||
        (c==0xf0 && s[1]<0x90) || (c==0xf4 && s[1]>=0x90)) return 0;
    return k;
}
bool ui_utf8_validar(const char *s, size_t *cantidad) {
    if (!s) return false;
    size_t n=strlen(s),i=0,total=0;
    while(i<n) {
        size_t k=ancho((const unsigned char *)s+i,n-i);
        if (!k) return false;
        i+=k; total++;
    }
    if (cantidad) *cantidad=total;
    return true;
}
bool ui_utf8_copiar_seguro(char *dst, size_t capacidad, const char *src) {
    if (!dst || !capacidad) return false;
    if (!ui_utf8_validar(src,NULL)) { dst[0]='\0'; return false; }
    size_t n=strlen(src),i=0;
    while(i<n) {
        size_t k=ancho((const unsigned char *)src+i,n-i);
        if (k>capacidad-1-i) break;
        i+=k;
    }
    memmove(dst,src,i); dst[i]='\0';
    return i==n;
}
bool ui_utf8_agregar(char *dst, size_t capacidad, unsigned int p) {
    if (!dst || !capacidad || !p || p>0x10ffff || (p>=0xd800 && p<=0xdfff)) return false;
    /* No leer fuera de un buffer sin terminador. */
    size_t n=0; while(n<capacidad && dst[n]) n++;
    if (n==capacidad || !ui_utf8_validar(dst,NULL)) return false;
    unsigned char t[4]; size_t k;
    if(p<0x80) { t[0]=(unsigned char)p; k=1; }
    else if(p<0x800) {t[0]=0xc0|(p>>6);t[1]=0x80|(p&63);k=2;}
    else if(p<0x10000) {t[0]=0xe0|(p>>12);t[1]=0x80|((p>>6)&63);t[2]=0x80|(p&63);k=3;}
    else {t[0]=0xf0|(p>>18);t[1]=0x80|((p>>12)&63);t[2]=0x80|((p>>6)&63);t[3]=0x80|(p&63);k=4;}
    if (k>capacidad-1-n) return false;
    memcpy(dst+n,t,k); dst[n+k]='\0'; return true;
}
bool ui_utf8_borrar_ultimo(char *s) {
    if (!ui_utf8_validar(s,NULL) || !s[0]) return false;
    size_t i=strlen(s)-1;
    while(i && (((unsigned char)s[i]&0xc0)==0x80)) i--;
    s[i]='\0'; return true;
}
