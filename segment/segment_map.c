#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

/*********************************/

#define XMIN        600000
#define XMAX        604000
#define YMIN        2424000
#define YMAX        2428000
#define XYSTEP      200
#define TSTEP       1800
#define REPEAT      25

/*********************************/

#define XCOUNT (((XMAX)-(XMIN))/(XYSTEP))
#define YCOUNT (((YMAX)-(YMIN))/(XYSTEP))
#define TCOUNT ((24*3600)/(TSTEP))

#define IDX(X) ((int)(((X)-XMIN)/XYSTEP))
#define IDY(Y) ((int)(((Y)-YMIN)/XYSTEP))
#define IDT(T) ((int)(((T)-day_start)/TSTEP))

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

/*********************************/

typedef struct segment {
    float x;
    float y;
    float r;
    time_t t;
    float nx;
    float ny;
    float nr;
    time_t nt;
} segment;

time_t day_start;
float presence[XCOUNT][YCOUNT][TCOUNT];

/*********************************/

static int parse_date(char *str){
    struct tm tm;
    strptime(str,"%Y-%m-%d %H:%M:%S", &tm);
    return timegm(&tm);
}

static const char * timetodate(time_t t){
    static char date[64];
    struct tm *tm = gmtime(&t);
    strftime(date, 64,"%Y-%m-%d %H:%M:%S",tm);
    return (const char *)&date;
}

static float norm(segment *seg){
    float x2 = (seg->x - seg->nx)*(seg->x - seg->nx);
    float y2 = (seg->y - seg->ny)*(seg->y - seg->ny);
    return sqrt(x2+y2);
}


static float uniform_random(float a,float b) {
    double r = random();
    return a+ (b-a) * r / ((double)RAND_MAX + 1);
}

static void circle_random(float x,float y,float r,float *ptrx,float *ptry){
    float angle = uniform_random(0,2*3.1415);
    float radius  = uniform_random(0.,r);
    *ptrx = x + radius*cos(angle);
    *ptry = y + radius*sin(angle);
    return;
}



static void parse_line(char *line,unsigned int len,segment *s,float *scaling)
{
    char *param=line,*ptr=line;
    char *eol = line + len;
    int column_id = 0;
    // scan line until end of line
    while (ptr <= eol){
        // check for a parameter delimiter
        if ((*ptr == '\t') || (*ptr == '\n') || (*ptr == '\0'))
        {
            switch (column_id)
            {
                case 0:
                    // aimsi
                    break;
                case 1:
                    // dat_heur_debt
                    s->t = parse_date(param);
                    break;
                case 2:
                    // ndat_heur_debt
                    s->nt = parse_date(param);
                    break;
                case 3:
                    // mcc
                    break;
                case 4:
                    // calc_rayon
                    s->r = atof(param);
                    break;
                case 5:
                    // smooth_x
                    s->x = atof(param);
                    break;
                case 6:
                    // smooth_y
                    s->y = atof(param);
                    break;
                case 7:
                    // numr_cell
                    break;
                case 8:
                    // ncalc_rayon
                    s->nr = atoi(param);
                    break;
                case 9:
                    // nsmooth_x
                    s->nx = atof(param);
                    break;
                case 10:
                    // nsmooth_y
                    s->ny = atof(param);
                    break;
                case 11:
                    // scaling
                    *scaling = atof(param);
                    break;
            }
            column_id++;
            param = ptr+1;
        }
        ptr++;
    }
}

static void zero_presence(void){
    int i,j,t;
    for (i=0;i<XCOUNT;i++)
        for (j=0;j<YCOUNT;j++)
            for (t=0;t<TCOUNT;t++)
                presence[i][j][t] = 0.f;
    return;
}

static int intersect_points(segment *seg,segment *iseg){
    // return intersect_points as a segment iseg
    // -1 if no intersection
    // 
    float d = norm(seg);

    if (d > (seg->r + seg->nr))
        return -1;
    else if ((d < fabs(seg->r - seg->nr)) || (d == 0))
        return 0;

    float a = (seg->r*seg->r - seg->nr*seg->nr + d*d) / (2 * d);
    float h = sqrt(seg->r*seg->r - a*a);
    float p2x = seg->x + a*(seg->nx - seg->x)/d;
    float p2y = seg->y + a*(seg->ny - seg->y)/d;

    iseg->x = p2x + h * (seg->ny - seg->y) / d;
    iseg->y = p2y - h * (seg->nx - seg->x) / d;

    iseg->nx = p2x - h * (seg->ny - seg->y) / d;
    iseg->ny = p2y + h * (seg->nx - seg->x) / d;
    return 1;
}


static void randsample_moving_position(segment *seg,float p,float *ptrx,float *ptry){
    float xa,ya,xb,yb;
    circle_random(seg->x,seg->y,seg->r,&xa,&ya);
    circle_random(seg->nx,seg->ny,seg->nr,&xb,&yb);
    *ptrx = xa + (xa-xb)*p;
    *ptry = ya + (ya-yb)*p;
    return;
}

static int randsample_static_position(segment *seg,float *ptrx,float *ptry){
    // return 0 if no static position can be found, >0 instead with position *ptrx,*ptry
    segment iseg;
    float box[4];
    float x,y;
    segment sega,segb;

    int ret = intersect_points(seg,&iseg);

    if (ret < 0)
        return 0;
    if (ret == 0){
        // one circle is fully inside the other, let's take the smallest one
        if (seg->r <seg->nr){
            box[0] = seg->x - seg->r;
            box[1] = seg->x + seg->r;
            box[2] = seg->y - seg->r;
            box[3] = seg->y + seg->r;
        }else{
            box[0] = seg->nx - seg->nr;
            box[1] = seg->nx + seg->nr;
            box[2] = seg->ny - seg->nr;
            box[3] = seg->ny + seg->nr;
        }
    }else{
        // circle to circle intersection
        float hx = iseg.x+iseg.nx;
        float hy = iseg.y+iseg.ny;
        float d = norm(&iseg)/2.;
        box[0] = hx-d;
        box[1] = hx+d;
        box[2] = hy-d;
        box[3] = hy+d;
    }
    int retry = 0;
    sega.x = seg->x;
    sega.y = seg->y;
    segb.x = seg->x;
    segb.y = seg->y;

    while (retry++<25){
        x = uniform_random(box[0],box[1]);
        y = uniform_random(box[2],box[3]);
        sega.nx = x;
        sega.ny = y;
        segb.nx = x;
        segb.ny = y;
        float da = norm(&sega);
        float db = norm(&segb);
        if ((da<seg->r) && (db<seg->nr)){
            // x,y is in the intersection of 
            // circle(seg->x,seg->y,seg->r) and circle(seg->nx,seg->ny,seg->nr)
            *ptrx = x;
            *ptry = y;
            return 1;
        }
    }
    *ptrx = (box[0]+box[1])/2.;
    *ptry = (box[2]+box[3])/2.;
    return 2;
}


static void interpolate(segment *seg,int now,float *x,float *y){
    segment static_seg;
    if ((now < seg->t) || (now >=seg->nt))
        return;
    // compute percentage
    float p = (now - seg->t)*1.0f/(seg->nt-seg->t);

    // for a circle_to_cirle intersection 
    // leading to a static position insight
    // allow :
    // - a static drift speed of 1m.s-1
    // - a random walking with maximum displacement ~ sqrt(time)
    // - never exceeding twice the original cell radius
    static_seg = *seg;
    static_seg.r = MIN(seg->r + sqrt(now-seg->t)*1., 2. * seg->r);
    static_seg.nr = MIN(seg->nr + sqrt(now)*1., 2 * 2. * seg->nr);
    int ret = randsample_static_position(&static_seg,x,y);
    if (ret == 0){
        randsample_moving_position(seg,p,x,y);
    }
    return;
}

static void parse_file(char *filename){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char buff[64];
    const char * date;

    fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    char *c = filename;
    while (*c++!='.' ){}
    memcpy(buff,c-11,10);
    memcpy(buff+10," 05:00:00",9);
    buff[19]='\0';
    printf("day start = <%s>\n",buff);
    time_t day_start = parse_date(buff);

    while ((read = getline(&line, &len, fp)) != -1) 
    {
        segment seg;
        float scaling = 1.;
        parse_line(line,len,&seg,&scaling);
//        printf(">>> %f,%f,%f,%f,%f,%f,%f\n",seg.x,seg.y,seg.r,seg.nx,seg.ny,seg.nr,scaling);
//        printf(">>> %s\n",timetodate(seg.t));
//        printf(">>> %s\n",timetodate(seg.nt));
        time_t t_start,t;
        t_start = day_start + (seg.t - day_start + TSTEP - 1)/TSTEP*TSTEP;

        int rep;
        int ix,iy,it;

        for (t = t_start;t < seg.nt;t+=TSTEP)
        {
            it = IDT(t);

            if ((it>=TCOUNT) || (it<0))
                continue;
            date = timetodate(day_start);
            for(rep=0;rep<REPEAT;rep++)
            {
                float x=0,y=0;
                interpolate(&seg,t,&x,&y);
                ix = IDX(x);
                iy = IDY(y);

//                printf("<<< %f|%f|%s %d,%d,%d\n",x,y,date,ix,iy,it);
                if ((ix<XCOUNT) && (ix>=0) && (iy<YCOUNT) && (iy>=0))
                {
                    presence[ix][iy][it] += scaling/REPEAT;
                }
            }
        }
    }

    fclose(fp);
    if (line)
        free(line);
    return;
}

#if 0
static void test_intersection(void){
    segment is,os;

    is.x = 0;
    is.y = 0;
    is.nx = 1;
    is.ny = 1;
    is.r = 2;
    is.nr = 2;
    intersect_points(&is,&os);
    printf("Intersection:(%f,%f) (%f,%f)\n",os.x,os.y,os.nx,os.ny); 

    is.x = 0;
    is.y = 0;
    is.nx = 4;
    is.ny = 0;
    is.r = 2;
    is.nr = 2;
    intersect_points(&is,&os);
    printf("Single-point edge collision:(%f,%f) (%f,%f)\n",os.x,os.y,os.nx,os.ny);

    is.x = 0;
    is.y = 0;
    is.nx = 1;
    is.ny = 0;
    is.r = 5;
    is.nr = 2;
    printf("Wholly inside: %d\n", intersect_points(&is,&os));

    is.x = 0;
    is.y = 0;
    is.nx = 5;
    is.ny = 0;
    is.r = 2;
    is.nr = 2;
    printf("No collision: %d\n", intersect_points(&is,&os));
    return;
}

static void test_uniform(){
    int i;
    for (i=0;i<100;i++)
        printf("uniform value [15,30] = %f\n",uniform_random(15,30));
    return; 
}

static void test_date(){
    char *date = "2014-12-14 03:00:00";
    const char *pdate = NULL;
    time_t t = parse_date(date);
    pdate = timetodate(t);
    printf("%s == %s\n",date,pdate);
}
#endif

void test_presence(){
    float minv =  1000000000;
    float maxv = -1000000000;
    float avgv = 0;
    int x,y,t;
    for (x=0;x<XCOUNT;x++)
        for (y=0;y<YCOUNT;y++)
            for (t=0;t<TCOUNT;t++)
    {
        minv = MIN(minv,presence[x][y][t]);
        maxv = MAX(maxv,presence[x][y][t]);
        avgv += presence[x][y][t];
    }
    avgv /= XCOUNT*YCOUNT*TCOUNT;
    printf("stat presence min=%f, max=%f, average=%f\n",minv,maxv,avgv);
}

int main(int argc,char *argv[])
{
    //test_date();
    //test_intersection();
    //test_uniform();

    zero_presence();
    parse_file("/home/ngaude/workspace/data/arzephir_italy_place_segment_2014-05-19.tsv");
    test_presence();
    return 0;
}



