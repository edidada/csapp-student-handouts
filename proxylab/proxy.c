/* Concurrent HTTP/1.0 GET proxy with a byte-bounded approximate-LRU cache.
 * Readers copy cached responses under a shared rwlock, then release it before
 * socket I/O. Atomic recency stamps allow simultaneous readers; writers alone
 * insert/evict. Network failures affect only the detached connection thread.
 */
#include "csapp.h"
#include <stdatomic.h>
#include <stdint.h>
#include <ctype.h>
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define REQUEST_LIMIT 65536
#define HEADER_LIMIT 128
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
typedef struct entry {
    char *key;
    unsigned char *data;
    size_t size;
    atomic_uint_fast64_t touched;
    struct entry *next;
} entry;
static entry *cache;
static size_t cache_bytes;
static pthread_rwlock_t cache_lock = PTHREAD_RWLOCK_INITIALIZER;
static atomic_uint_fast64_t ticks;
typedef struct { char *line, *value; size_t name; } header;

/* Copy while protected against eviction; never hold the lock over slow I/O. */
static unsigned char *cache_get(const char *key, size_t *size)
{
    entry *p; unsigned char *copy = NULL;
    pthread_rwlock_rdlock(&cache_lock);
    for (p=cache;p;p=p->next) if (!strcmp(p->key,key)) {
        copy=malloc(p->size);
        if(copy) { memcpy(copy,p->data,p->size); *size=p->size;
            atomic_store(&p->touched,atomic_fetch_add(&ticks,1)+1); }
        break;
    }
    pthread_rwlock_unlock(&cache_lock);
    return copy;
}

/* Allocate before locking, deduplicate concurrent misses and evict oldest use. */
static void cache_put(const char *key, const unsigned char *data, size_t size)
{
    entry *p, *node=calloc(1,sizeof(*node));
    if(!node) return;
    node->key=strdup(key); node->data=malloc(size); node->size=size;
    if(!node->key || !node->data) { free(node->key); free(node->data); free(node); return; }
    memcpy(node->data,data,size);
    pthread_rwlock_wrlock(&cache_lock);
    for(p=cache;p;p=p->next) if(!strcmp(p->key,key)) {
        atomic_store(&p->touched,atomic_fetch_add(&ticks,1)+1);
        pthread_rwlock_unlock(&cache_lock);
        free(node->key); free(node->data); free(node); return;
    }
    while(cache_bytes+size>MAX_CACHE_SIZE) {
        entry **slot=&cache, **oldest=&cache;
        for(;*slot;slot=&(*slot)->next)
            if(atomic_load(&(*slot)->touched)<atomic_load(&(*oldest)->touched)) oldest=slot;
        p=*oldest; *oldest=p->next; cache_bytes-=p->size;
        free(p->key); free(p->data); free(p);
    }
    atomic_init(&node->touched,atomic_fetch_add(&ticks,1)+1);
    node->next=cache; cache=node; cache_bytes+=size;
    pthread_rwlock_unlock(&cache_lock);
}

/* Small fixed error responses; raw RIO returns errors instead of exiting. */
static void error_reply(int fd, int status, const char *reason)
{
    char buf[512]; int n=snprintf(buf,sizeof(buf),"HTTP/1.0 %d %s\r\nConnection: close\r\nContent-Length: 0\r\n\r\n",status,reason);
    rio_writen(fd,buf,(size_t)n);
}
static int named(const header *h,const char *name)
{ return h->name==strlen(name) && !strncasecmp(h->line,name,h->name); }

/* Trim a single header value without changing the original forwarding bytes. */
static int value_of(const header *h,char *out,size_t cap)
{
    const char *end=h->line+strlen(h->line), *start=h->value;
    while(end>start && isspace((unsigned char)end[-1])) end--;
    while(start<end && (*start==' '||*start=='\t')) start++;
    if((size_t)(end-start)>=cap) return 0;
    memcpy(out,start,(size_t)(end-start)); out[end-start]=0; return 1;
}

/* Parse hostname[:port], including bracketed IPv6, without userinfo/controls. */
static int authority(const char *input,char *host,char *port)
{
    char buf[1024], *end, *colon; long number; size_t i;
    if(!*input || strlen(input)>=sizeof(buf)) return 0;
    strcpy(buf,input);
    for(i=0;buf[i];i++) if(isspace((unsigned char)buf[i])||strchr("/@?#\\",buf[i])) return 0;
    strcpy(port,"80");
    if(buf[0]=='[') {
        end=strchr(buf,']'); if(!end || end==buf+1) return 0;
        *end=0; strcpy(host,buf+1);
        if(end[1]) { if(end[1]!=':'||!end[2]) return 0; if(strlen(end+2)>5) return 0; strcpy(port,end+2); }
    } else {
        colon=strchr(buf,':');
        if(colon) { if(strchr(colon+1,':')||!colon[1]||strlen(colon+1)>5) return 0; *colon=0; strcpy(port,colon+1); }
        if(!*buf) return 0;
        strcpy(host,buf);
    }
    for(i=0;port[i];i++) if(!isdigit((unsigned char)port[i])) return 0;
    number=strtol(port,&end,10); return !*end && number>0 && number<=65535;
}

/* Safe append preserving extra request fields, bounded across all headers. */
static int append(char *buf,size_t *len,const char *text)
{
    size_t n=strlen(text);
    if(n>=REQUEST_LIMIT-*len) return 0;
    memcpy(buf+*len,text,n+1); *len+=n; return 1;
}
static int connection_named(const char *list,const header *h)
{
    const char *p=list,*start,*end;
    while(*p) {
        while(*p==' '||*p=='\t'||*p==',') p++;
        start=p; while(*p&&*p!=',') p++; end=p;
        while(end>start&&isspace((unsigned char)end[-1])) end--;
        if((size_t)(end-start)==h->name&&!strncasecmp(start,h->line,h->name)) return 1;
    }
    return 0;
}

/* Portable case-insensitive substring search without GNU netdb extensions. */
static int contains_case(const char *text,const char *needle)
{
    size_t n=strlen(needle);
    for(;*text;text++) if(!strncasecmp(text,needle,n)) return 1;
    return 0;
}

/* Cache only complete successful responses; a truncated declared body must
 * never turn into a successful cached object. Full HTTP freshness is outside
 * this lab, but private/no-store, transfer-coded and Vary:* replies bypass. */
static int cacheable(unsigned char *object,size_t n)
{
    char *end,*line,*nextline, saved, *colon, *tail;
    size_t header_bytes,declared=0; int have_length=0,status;
    object[n]=0;
    end=strstr((char *)object,"\r\n\r\n"); if(!end) return 0;
    header_bytes=(size_t)(end-(char *)object)+4;
    if(memchr(object,0,header_bytes)) return 0;
    if(sscanf((char *)object,"HTTP/%*s %d",&status)!=1 || status!=200) return 0;
    line=strstr((char *)object,"\r\n")+2;
    while(line<end) {
        nextline=strstr(line,"\r\n"); if(!nextline) return 0;
        saved=*nextline; *nextline=0; colon=strchr(line,':');
        if(!colon) { *nextline=saved; return 0; }
        *colon=0;
        if(!strcasecmp(line,"Content-Length")) {
            unsigned long long v; char *value=colon+1;
            while(*value==' '||*value=='\t') value++;
            errno=0; v=strtoull(value,&tail,10); while(*tail==' '||*tail=='\t') tail++;
            if(!isdigit((unsigned char)*value)||*tail||errno||v>SIZE_MAX|| (have_length&&declared!=(size_t)v)) {
                *colon=':'; *nextline=saved; return 0;
            }
            have_length=1; declared=(size_t)v;
        } else if(!strcasecmp(line,"Transfer-Encoding") ||
          (!strcasecmp(line,"Cache-Control")&&(contains_case(colon+1,"no-store")||contains_case(colon+1,"private")||contains_case(colon+1,"no-cache"))) ||
          (!strcasecmp(line,"Vary")&&strchr(colon+1,'*'))) {
            *colon=':'; *nextline=saved; return 0;
        }
        *colon=':'; *nextline=saved; line=nextline+2;
    }
    return !have_length || declared==n-header_bytes;
}

/* Read one complete GET request and forward a bounded HTTP/1.0 transaction. */
static void transaction(int fd)
{
    rio_t input; header fields[HEADER_LIMIT];
    char line[MAXLINE],method[16],uri[MAXLINE],version[16],extra;
    char storage[REQUEST_LIMIT],request[REQUEST_LIMIT],connections[MAXLINE]="";
    char host[1024],port[6],host_value[1024]="",target[MAXLINE],url_host[1024];
    size_t stored=0,length=0,count=0,i,n,size=0; ssize_t got;
    int origin=-1,has_host=0,bypass=0,complete=0; unsigned char *object=NULL,*hit;
    rio_readinitb(&input,fd);
    got=rio_readlineb(&input,line,sizeof(line)); if(got<=0) return;
    if((size_t)got==sizeof(line)-1 || !strstr(line,"\r\n") ||
       sscanf(line,"%15s %8191s %15s %c",method,uri,version,&extra)!=3) { error_reply(fd,400,"Bad Request"); return; }
    if(strcmp(method,"GET")) { error_reply(fd,501,"Not Implemented"); return; }
    if(strcmp(version,"HTTP/1.0")&&strcmp(version,"HTTP/1.1")) { error_reply(fd,400,"Bad Request"); return; }
    for(;;) {
        char *colon,*v; size_t j;
        got=rio_readlineb(&input,line,sizeof(line));
        if(got<=0 || (size_t)got==sizeof(line)-1 || !strstr(line,"\r\n")) { error_reply(fd,400,"Bad Request"); return; }
        if(!strcmp(line,"\r\n")) break;
        n=(size_t)got;
        if(count==HEADER_LIMIT || n+1>sizeof(storage)-stored) { error_reply(fd,431,"Request Header Fields Too Large"); return; }
        colon=strchr(line,':');
        if(!colon||colon==line) { error_reply(fd,400,"Bad Request"); return; }
        for(j=0;j<(size_t)(colon-line);j++) if(!isalnum((unsigned char)line[j])&&!strchr("!#$%&'*+-.^_`|~",line[j])) { error_reply(fd,400,"Bad Request"); return; }
        for(v=colon+1;v<line+n-2;v++) if(((unsigned char)*v<32&&*v!='\t')||*v==127) { error_reply(fd,400,"Bad Request"); return; }
        memcpy(storage+stored,line,n+1); fields[count]=(header){storage+stored,storage+stored+(colon-line)+1,(size_t)(colon-line)};
        if(named(&fields[count],"Host")) {
            if(has_host++ || !value_of(&fields[count],host_value,sizeof(host_value))) { error_reply(fd,400,"Bad Request"); return; }
        }
        if(named(&fields[count],"Connection")) {
            char value[MAXLINE]; if(!value_of(&fields[count],value,sizeof(value)) || strlen(connections)+strlen(value)+2>=sizeof(connections)) { error_reply(fd,431,"Request Header Fields Too Large"); return; }
            strcat(connections,","); strcat(connections,value);
        }
        if(named(&fields[count],"Transfer-Encoding")) { error_reply(fd,400,"Bad Request"); return; }
        if(named(&fields[count],"Content-Length")) {
            char value[32]; if(!value_of(&fields[count],value,sizeof(value))||strcmp(value,"0")) { error_reply(fd,400,"Bad Request"); return; }
        }
        if(named(&fields[count],"Authorization")||named(&fields[count],"Cookie")||named(&fields[count],"Cache-Control")||named(&fields[count],"Range")) bypass=1;
        stored+=n+1; count++;
    }
    if(!strncasecmp(uri,"http://",7)) {
        char *path=strpbrk(uri+7,"/?#"); n=path?(size_t)(path-uri-7):strlen(uri+7);
        if(n>=sizeof(url_host)) { error_reply(fd,400,"Bad Request"); return; }
        memcpy(url_host,uri+7,n); url_host[n]=0;
        if(!authority(url_host,host,port)) { error_reply(fd,400,"Bad Request"); return; }
        if(!path) strcpy(target,"/");
        else if(*path=='?') snprintf(target,sizeof(target),"/%s",path);
        else { if(*path=='#') { error_reply(fd,400,"Bad Request"); return; } strcpy(target,path); }
        if(!has_host) strcpy(host_value,url_host);
    } else if(uri[0]=='/' && has_host && authority(host_value,host,port)) strcpy(target,uri);
    else { error_reply(fd,400,"Bad Request"); return; }
    if(strchr(target,'#')||strchr(target,'\\')) { error_reply(fd,400,"Bad Request"); return; }
    if(!append(request,&length,"GET ")||!append(request,&length,target)||!append(request,&length," HTTP/1.0\r\nHost: ")||!append(request,&length,host_value)||!append(request,&length,"\r\n")||!append(request,&length,user_agent_hdr)||!append(request,&length,"Connection: close\r\nProxy-Connection: close\r\n")) { error_reply(fd,431,"Request Header Fields Too Large"); return; }
    for(i=0;i<count;i++) {
        header *h=&fields[i];
        if(named(h,"Host")||named(h,"User-Agent")||named(h,"Connection")||named(h,"Proxy-Connection")||named(h,"Keep-Alive")||named(h,"TE")||named(h,"Trailer")||named(h,"Upgrade")||named(h,"Proxy-Authorization")||connection_named(connections,h)) continue;
        if(!append(request,&length,h->line)) { error_reply(fd,431,"Request Header Fields Too Large"); return; }
    }
    if(!append(request,&length,"\r\n")) { error_reply(fd,431,"Request Header Fields Too Large"); return; }
    /* The key includes actual origin authority as well as all forwarded fields,
     * preventing collisions from Host overrides and request-header variants. */
    { char *key=malloc(length+strlen(host)+strlen(port)+4);
      if(!key) { error_reply(fd,503,"Service Unavailable"); return; }
      sprintf(key,"%s:%s\n%s",host,port,request);
      hit=bypass?NULL:cache_get(key,&size);
      if(hit) { rio_writen(fd,hit,size); free(hit); free(key); return; }
      origin=open_clientfd(host,port);
      if(origin<0) { error_reply(fd,502,"Bad Gateway"); free(key); return; }
      { struct timeval timeout={10,0}; setsockopt(origin,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)); setsockopt(origin,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)); }
      if(rio_writen(origin,request,length)<0) { close(origin); free(key); return; }
      if(!bypass) object=malloc(MAX_OBJECT_SIZE+1);
      for(;;) {
          got=read(origin,line,sizeof(line));
          if(got<0&&errno==EINTR) continue;
          if(got<=0) { complete=got==0; break; }
          if(rio_writen(fd,line,(size_t)got)<0) break;
          if(object) {
              if((size_t)got>MAX_OBJECT_SIZE-size) { free(object); object=NULL; }
              else { memcpy(object+size,line,(size_t)got); size+=(size_t)got; }
          }
      }
      close(origin);
      if(object && complete && cacheable(object,size)) cache_put(key,object,size);
      free(object); free(key);
    }
}

/* Detached threads own their descriptors and argument allocations. */
static void *worker(void *arg)
{
    int fd=*(int *)arg; struct timeval timeout={10,0}; free(arg);
    pthread_detach(pthread_self());
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    transaction(fd); close(fd); return NULL;
}
int main(int argc,char **argv)
{
    int listenfd,fd,*argument; pthread_t thread;
    if(argc!=2) { fprintf(stderr,"usage: %s <port>\n",argv[0]); return 1; }
    signal(SIGPIPE,SIG_IGN);
    listenfd=open_listenfd(argv[1]); if(listenfd<0) return 1;
    for(;;) {
        fd=accept(listenfd,NULL,NULL);
        if(fd<0) { if(errno==EINTR) continue; perror("accept"); continue; }
        argument=malloc(sizeof(*argument));
        if(!argument) { close(fd); continue; }
        *argument=fd;
        if(pthread_create(&thread,NULL,worker,argument)) { close(fd); free(argument); }
    }
}
