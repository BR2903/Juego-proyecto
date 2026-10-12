#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <stdbool.h>

#define ESPECIES 3
#define VACIO 0
#define MAX_PARTIDAS 5
#define MAX_NOMBRE 32
#define MIN_FILAS 5
#define MIN_COLUMNAS 5
#define MAX_FILAS 200
#define MAX_COLUMNAS 200

static const char* JSON_PATH = "partidas.json";

typedef struct {
    int filas;
    int columnas;
    int *mundo;
    int *edad;               // edad por celda (no se persiste)
    char nombre[MAX_NOMBRE];
} Partida;

#define IDX(p,i,j) ((i) * (p)->columnas + (j))

//==================== Doble buffer de consola (sin flicker) ==================
typedef struct {
    HANDLE front, back;     
    HANDLE primary;         
    int buf_w, buf_h;       
    CHAR_INFO* canvas;      
} DBConsole;

static DBConsole gdb = {0};

// Prototipos generales
void avanzar_turno(Partida *p);
void menu_inicial();
void nueva_partida();
void cargar_partida();
void editar_mundo(Partida *p);
void simular(Partida *p);
void limpiarMundo(Partida *p);
void liberar_partida(Partida *p);
void liberar_todas_las_partidas();
int leer_entero_en_rango(const char *prompt, int minv, int maxv);
int menu_simple(const char* titulo, const char* opciones[], int num_opciones);

// Persistencia JSON
int guardar_partidas_json(const char* path, Partida partidas[], int n);
int cargar_partidas_json(const char* path, Partida partidas[], int *n);

// Helpers DB
static void hide_cursor(HANDLE h, BOOL vis) {
    CONSOLE_CURSOR_INFO ci = { .bVisible = vis, .dwSize = 100 };
    SetConsoleCursorInfo(h, &ci);
}
static int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}// typo-safe

static void get_active_window_size(int* out_w, int* out_h) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!gdb.front || !GetConsoleScreenBufferInfo(gdb.front, &csbi)) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleScreenBufferInfo(h, &csbi)) { *out_w=120; *out_h=30; return; }
    }
    SMALL_RECT r = csbi.srWindow;
    *out_w = (r.Right - r.Left + 1);
    *out_h = (r.Bottom - r.Top + 1);
}

static int db_ensure_buffers_at_least(int want_w, int want_h) {
    int grow = 0;
    if (!gdb.front || !gdb.back) grow = 1;
    if (!grow && (want_w > gdb.buf_w || want_h > gdb.buf_h)) grow = 1;
    if (!grow) return 1;

    if (gdb.front) CloseHandle(gdb.front);
    if (gdb.back)  CloseHandle(gdb.back);

    gdb.front = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    gdb.back  = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (gdb.front == INVALID_HANDLE_VALUE || gdb.back == INVALID_HANDLE_VALUE) {
        gdb.front = gdb.back = NULL; return 0;
    }

    COORD size = { (SHORT)want_w, (SHORT)want_h };
    SetConsoleScreenBufferSize(gdb.front, size);
    SetConsoleScreenBufferSize(gdb.back,  size);

    SetConsoleActiveScreenBuffer(gdb.front);
    hide_cursor(gdb.front, FALSE);
    hide_cursor(gdb.back,  FALSE);

    gdb.buf_w = want_w;
    gdb.buf_h = want_h;

    free(gdb.canvas);
    gdb.canvas = (CHAR_INFO*)calloc((size_t)gdb.buf_w*(size_t)gdb.buf_h, sizeof(CHAR_INFO));
    return 1;
}

static void db_enter(void) {
    if (!gdb.primary) gdb.primary = GetStdHandle(STD_OUTPUT_HANDLE);

    int vis_w, vis_h;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(gdb.primary, &csbi)) {
        SMALL_RECT r = csbi.srWindow;
        vis_w = (r.Right - r.Left + 1);
        vis_h = (r.Bottom - r.Top + 1);
    } else { vis_w=120; vis_h=30; }

    vis_w = clampi(vis_w, 20, 500);
    vis_h = clampi(vis_h, 10, 500);
    db_ensure_buffers_at_least(vis_w, vis_h);
}

static void db_leave(void) {
    if (gdb.primary) SetConsoleActiveScreenBuffer(gdb.primary);
    if (gdb.front) CloseHandle(gdb.front);
    if (gdb.back)  CloseHandle(gdb.back);
    gdb.front = gdb.back = NULL;
    gdb.buf_w = gdb.buf_h = 0;
    free(gdb.canvas); gdb.canvas = NULL;
}

static void db_begin_frame(int content_w, int content_h, int* out_w_used, int* out_h_used) {
    int vis_w=0, vis_h=0;
    if (gdb.front) get_active_window_size(&vis_w,&vis_h);
    else { vis_w = 120; vis_h = 30; }

    int w = content_w > vis_w ? content_w : vis_w;
    int h = content_h > vis_h ? content_h : vis_h;

    w = clampi(w, 20, 500);
    h = clampi(h, 10, 500);

    db_ensure_buffers_at_least(w, h);

    size_t n = (size_t)w*(size_t)h;
    for (size_t i=0;i<n;i++){ gdb.canvas[i].Char.AsciiChar=' '; gdb.canvas[i].Attributes=7; }

    *out_w_used = w;
    *out_h_used = h;
}

static void db_present(int w_used, int h_used) {
    COORD bufSize = { (SHORT)w_used, (SHORT)h_used };
    COORD bufCoord = {0,0};
    SMALL_RECT rect = {0,0,(SHORT)(w_used-1),(SHORT)(h_used-1)};
    WriteConsoleOutputA(gdb.back, gdb.canvas, bufSize, bufCoord, &rect);

    HANDLE tmp = gdb.front; gdb.front = gdb.back; gdb.back = tmp;
    SetConsoleActiveScreenBuffer(gdb.front);
}

static void put_text(int x,int y,const char* s, WORD attr, int w, int h) {
    if (y<0||y>=h) return;
    int i=0;
    while (s[i] && x+i<w) {
        if (x+i>=0) {
            gdb.canvas[y*w + (x+i)].Char.AsciiChar = s[i];
            gdb.canvas[y*w + (x+i)].Attributes = attr;
        }
        ++i;
    }
}

//================= Parámetros biológicos de las especies =====================
// Prob. de reproducción por turno si hay pareja y hueco alrededor
// A: normal, B: normal, C: más alta (pacifista pero se reproduce más)
static float fecundidad[ESPECIES+1]       = { 0.0f, 0.18f, 0.15f, 0.24f };
// Prob. de muerte natural por turno (distinta para cada especie)
// A: más mortal, B: normal, C: menos mortal
static float longevidad_muerte[ESPECIES+1]= { 0.0f, 0.07f, 0.04f, 0.02f };
// Tabla original de agresividad (ya no se usa para probabilidades, pero se mantiene)
static float agresividad[ESPECIES+1]      = { 0.0f, 1.0f, 0.0f, 0.0f };

int menu_simple(const char* titulo, const char* opciones[], int num_opciones) {
    int opcion;
    while (1) {
        system("cls");
        printf("=== %s ===\n\n", titulo);
        for (int i=0;i<num_opciones;i++) printf("%d. %s\n", i+1, opciones[i]);
        printf("\nOpcion: ");
        if (scanf("%d", &opcion) == 1) {
            getchar();
            if (opcion>=1 && opcion<=num_opciones) return opcion-1;
        } else { int c; while((c=getchar())!='\n' && c!=EOF){} }
        printf("\nOpcion invalida. Enter para continuar..."); getchar();
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    menu_inicial();
    liberar_todas_las_partidas();
    return 0;
}

void menu_inicial() {
    const char* opciones[]={"Nueva Partida","Cargar Partida","Salir"};
    int op;
    do {
        op = menu_simple("SIMULADOR DE VIDA", opciones, 3);
        if (op==0) nueva_partida();
        else if (op==1) cargar_partida();
        else { printf("Adios!\n"); Sleep(600); }
    } while (op!=2);
}

int leer_entero_en_rango(const char *prompt, int minv, int maxv) {
    int v;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d",&v)==1) {
            getchar();
            if (v>=minv && v<=maxv) return v;
        } else { int c; while((c=getchar())!='\n' && c!=EOF){} }
        printf("Valor invalido. Debe estar entre %d y %d.\n", minv, maxv);
    }
}

Partida partidas[MAX_PARTIDAS];
int n_partidas = 0;

void nueva_partida() {
    if (n_partidas >= MAX_PARTIDAS) { system("cls"); printf("Maximo de partidas alcanzado.\n"); Sleep(1200); return; }
    Partida *p = &partidas[n_partidas];

    const char* tamanos[]={
        "Pequeno (10x10)","Mediano (25x25)","Grande (50x50)","Muy Grande (100x100)","Personalizado","Volver"};
    int size_choice = menu_simple("CONFIGURAR TAMANO DEL MUNDO", tamanos, 6);
    switch (size_choice) {
        case 0: p->filas=10;  p->columnas=10; break;
        case 1: p->filas=25;  p->columnas=25; break;
        case 2: p->filas=50;  p->columnas=50; break;
        case 3: p->filas=100; p->columnas=100; break;
        case 4:
            system("cls");
            p->filas    = leer_entero_en_rango("Filas (5-200): ", MIN_FILAS, MAX_FILAS);
            p->columnas = leer_entero_en_rango("Columnas (5-200): ", MIN_COLUMNAS, MAX_COLUMNAS);
            break;
        default: return;
    }

    size_t total = (size_t)p->filas * (size_t)p->columnas;
    p->mundo = (int*)malloc(total*sizeof(int));
    p->edad  = (int*)calloc(total,sizeof(int));
    if (!p->mundo || !p->edad) { printf("Memoria insuficiente.\n"); if(p->mundo)free(p->mundo); if(p->edad)free(p->edad); Sleep(1200); return; }

    system("cls");
    printf("Nombre de la partida (max 31 caracteres): ");
    if (!fgets(p->nombre, MAX_NOMBRE, stdin)) strcpy(p->nombre,"Partida");
    p->nombre[strcspn(p->nombre,"\n")] = 0;

    limpiarMundo(p);
    editar_mundo(p);
    simular(p);

    n_partidas++;
    if (guardar_partidas_json(JSON_PATH, partidas, n_partidas) != 0) { printf("No se pudo guardar en JSON.\n"); Sleep(1000); }
}

void cargar_partida() {
    liberar_todas_las_partidas();
    if (cargar_partidas_json(JSON_PATH, partidas, &n_partidas) != 0 || n_partidas==0) {
        system("cls"); printf("No hay partidas guardadas.\n"); Sleep(1500); return;
    }
    for (int i=0;i<n_partidas;i++) {
        size_t total = (size_t)partidas[i].filas * (size_t)partidas[i].columnas;
        partidas[i].edad = (int*)calloc(total,sizeof(int));
    }

    system("cls");
    printf("Partidas disponibles:\n\n");
    for (int i=0;i<n_partidas;i++) printf("%d. %s (%dx%d)\n", i+1, partidas[i].nombre, partidas[i].filas, partidas[i].columnas);
    printf("%d. Volver\n", n_partidas+1);
    int choice = leer_entero_en_rango("\nSeleccione: ", 1, n_partidas+1);
    if (choice==n_partidas+1) return;
    simular(&partidas[choice-1]);

    if (guardar_partidas_json(JSON_PATH, partidas, n_partidas) != 0) { printf("No se pudo guardar en JSON.\n"); Sleep(1000); }
}

static int textlen(const char* s){ return (int)strlen(s); }

// Colores base sin intensidad; adultos añaden intensidad para “brillar”
#define FG_R FOREGROUND_RED
#define FG_G FOREGROUND_GREEN
#define FG_B FOREGROUND_BLUE
#define FG_I FOREGROUND_INTENSITY
static WORD COL_DOT   = 8;           // gris para puntos vacíos
static WORD COL_DEF   = 7;           // texto UI
static WORD COL_A_DIM = FG_G;        // A joven = verde
static WORD COL_B_DIM = FG_R;        // B joven = rojo
static WORD COL_C_DIM = FG_G|FG_B;   // C joven = cian
static WORD BG_SEL    = BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY; // cursor resaltado
static const int ADULTO_UMBRAL = 4;  // a partir de esta edad “brillan” más

static void draw_grid_into_canvas(const Partida* p, int highlight_i, int highlight_j,
                                  int start_x, int start_y, int w, int h)
{
    for (int i=0;i<p->filas;i++) {
        int x = start_x; int y = start_y + i;
        for (int j=0;j<p->columnas;j++) {
            int val = p->mundo[IDX(p,i,j)];
            WORD a = COL_DEF;
            char ch='?';

            if (val==VACIO) { ch='.'; a=COL_DOT; }
            else if (val==1){ ch='A'; a=COL_A_DIM; }
            else if (val==2){ ch='B'; a=COL_B_DIM; }
            else if (val==3){ ch='C'; a=COL_C_DIM; }

            if (p->edad && val!=VACIO && p->edad[IDX(p,i,j)]>=ADULTO_UMBRAL) a |= FG_I;
            if (i==highlight_i && j==highlight_j) a = (a & 0x0F) | BG_SEL;

            int idx = y*w + x;
            if (y>=0 && y<h && x>=0 && x+1<w) {
                gdb.canvas[idx].Char.AsciiChar = ch;
                gdb.canvas[idx].Attributes = a;
                gdb.canvas[idx+1].Char.AsciiChar = ' ';
                gdb.canvas[idx+1].Attributes = a & 0x0F;
            }
            x += 2;
        }
    }
}

static void draw_sim_frame(const Partida* p, bool autoplay, int delay_ms) {
    char h0[256]; snprintf(h0,sizeof(h0),"Simulando partida '%s' (%dx%d)", p->nombre, p->filas, p->columnas);

    int c1=0,c2=0,c3=0;
    for (int i=0;i<p->filas;i++) for (int j=0;j<p->columnas;j++){
        int v=p->mundo[IDX(p,i,j)];
        if (v==1) c1++; else if (v==2) c2++; else if (v==3) c3++;
    }
    char s1[256]; snprintf(s1,sizeof(s1),"Poblacion: A=%d  B=%d  C=%d  (Total=%d)", c1,c2,c3,c1+c2+c3);
    double tps = (delay_ms>0) ? (1000.0/delay_ms) : 0.0;
    char s2[256]; snprintf(s2,sizeof(s2),"AUTO: %s | Velocidad: %.2f tps (%d ms) | Controles: [SPACE] auto  [+/-] vel  [1] 1t  [2] 10t  [N] Nt  [E] evento  [3] editar  [4] salir",
                           autoplay? "ON" : "OFF", tps, delay_ms);

    int grid_w=p->columnas*2;
    int content_w=grid_w;
    if (textlen(h0)>content_w) content_w=textlen(h0);
    if (textlen(s1)>content_w) content_w=textlen(s1);
    if (textlen(s2)>content_w) content_w=textlen(s2);
    int content_h=2 + p->filas + 3;

    int w_used=0,h_used=0;
    db_begin_frame(content_w, content_h, &w_used, &h_used);

    put_text(0,0,h0,COL_DEF,w_used,h_used);
    draw_grid_into_canvas(p, -1, -1, 0, 2, w_used, h_used);
    put_text(0, 2+p->filas,   s1, COL_DEF, w_used, h_used);
    put_text(0, 2+p->filas+1, s2, COL_DEF, w_used, h_used);

    db_present(w_used,h_used);
}

void editar_mundo(Partida *p) {
    int cx=0, cy=0;
    db_enter();
    for (int editing=1; editing; ) {
        // Frame de edición
        char l0[256], l1[256], l2[160];
        snprintf(l0,sizeof(l0),"Edicion del mundo '%s' (%dx%d)", p->nombre, p->filas, p->columnas);
        snprintf(l1,sizeof(l1),"Cursor: (%d,%d)  | Coloca con 1/2/3, borra con 0", cx, cy);
        snprintf(l2,sizeof(l2),"Controles: WASD-mover, 1/2/3-colocar, 0-eliminar, L-limpiar, ESC-salir");

        int grid_w = p->columnas*2;
        int content_w = grid_w;
        if ((int)strlen(l0)>content_w) content_w=(int)strlen(l0);
        if ((int)strlen(l1)>content_w) content_w=(int)strlen(l1);
        if ((int)strlen(l2)>content_w) content_w=(int)strlen(l2);
        int content_h = 3 + p->filas + 2;

        int w_used=0,h_used=0;
        db_begin_frame(content_w, content_h, &w_used, &h_used);

        put_text(0,0,l0,COL_DEF,w_used,h_used);
        put_text(0,1,l1,COL_DEF,w_used,h_used);
        draw_grid_into_canvas(p, cx, cy, 0, 3, w_used, h_used);
        put_text(0, 3+p->filas+1, l2, COL_DEF, w_used, h_used);

        db_present(w_used,h_used);

        int k = _getch();
        switch (k) {
            case 'w': case 'W': if (cx>0) cx--; break;
            case 's': case 'S': if (cx<p->filas-1) cx++; break;
            case 'a': case 'A': if (cy>0) cy--; break;
            case 'd': case 'D': if (cy<p->columnas-1) cy++; break;
            case '1': p->mundo[IDX(p,cx,cy)] = 1; if(p->edad) p->edad[IDX(p,cx,cy)]=0; break;
            case '2': p->mundo[IDX(p,cx,cy)] = 2; if(p->edad) p->edad[IDX(p,cx,cy)]=0; break;
            case '3': p->mundo[IDX(p,cx,cy)] = 3; if(p->edad) p->edad[IDX(p,cx,cy)]=0; break;
            case '0': p->mundo[IDX(p,cx,cy)] = VACIO; if(p->edad) p->edad[IDX(p,cx,cy)]=0; break;
            case 'l': case 'L': {
                size_t total=(size_t)p->filas*(size_t)p->columnas;
                for (size_t k2=0;k2<total;k2++){ p->mundo[k2]=VACIO; if(p->edad)p->edad[k2]=0; }
            } break;
            case 27: editing = 0; break;
        }
    }
    db_leave();
}

void simular(Partida *p) {
    db_enter();
    bool autoplay = false;
    int delay_ms = 120;              
    const int delay_min = 10;        
    const int delay_max = 2000;      
    ULONGLONG last_tick = GetTickCount64();

    for (int running=1; running; ) {

        // Avance automático si corresponde
        if (autoplay) {
            ULONGLONG now = GetTickCount64();
            if (now - last_tick >= (ULONGLONG)delay_ms) {
                avanzar_turno(p);
                last_tick = now;
            }
        }

        // Dibujo del frame de simulación (muestra estado de autoplay/velocidad)
        draw_sim_frame(p, autoplay, delay_ms);

        // Lectura no bloqueante de teclado
        if (_kbhit()) {
            int tecla = _getch();
            switch (tecla) {
                case ' ': // SPACE: toggle autoplay
                    autoplay = !autoplay;
                    last_tick = GetTickCount64();
                    break;
                case '+': case '=': // más rápido (menor delay)
                    delay_ms -= (delay_ms >= 100) ? 50 : 5;
                    if (delay_ms < delay_min) delay_ms = delay_min;
                    break;
                case '-': // más lento (mayor delay)
                    delay_ms += (delay_ms >= 100) ? 50 : 5;
                    if (delay_ms > delay_max) delay_ms = delay_max;
                    break;

                case '1':
                    autoplay = false;
                    avanzar_turno(p);
                    last_tick = GetTickCount64();
                    break;

                case '2':
                    autoplay = false;
                    for (int t=0;t<10;t++) avanzar_turno(p);
                    last_tick = GetTickCount64();
                    break;

                case 'n': case 'N': {
                    autoplay = false;
                    db_leave();
                    int n = leer_entero_en_rango("Cuantos turnos avanzar?: ", 1, 100000000);
                    db_enter();
                    for (int t=0;t<n;t++) avanzar_turno(p);
                    last_tick = GetTickCount64();
                } break;

                case 'e': case 'E': {
                    autoplay = false;
                    db_leave();
                    const char* ops[]={"Plaga (-30% al azar)","Bendicion (+20%% nac. por 10 turnos)","Volver"};
                    int eop = menu_simple("EVENTO DIVINO", ops, 3);
                    if (eop==0) {
                        size_t total=(size_t)p->filas*(size_t)p->columnas;
                        for (size_t k=0;k<total;k++)
                            if (p->mundo[k]!=VACIO && (rand()/(float)RAND_MAX) < 0.30f) { p->mundo[k]=VACIO; if(p->edad) p->edad[k]=0; }
                    } else if (eop==1) {
                        for (int t=0;t<10;t++){
                            float a=fecundidad[1],b=fecundidad[2],c=fecundidad[3];
                            fecundidad[1]=a+0.10f; fecundidad[2]=b+0.10f; fecundidad[3]=c+0.10f;
                            avanzar_turno(p);
                            fecundidad[1]=a; fecundidad[2]=b; fecundidad[3]=c;
                        }
                    }
                    db_enter();
                    last_tick = GetTickCount64();
                } break;

                case '3':
                    autoplay = false;
                    db_leave(); editar_mundo(p); db_enter();
                    last_tick = GetTickCount64();
                    break;

                case '4':
                    running=0; break;
            }
        } else {
            // No hay teclado: si no hay autoplay, descansar un poco para no consumir CPU
            if (!autoplay) Sleep(10);
        }
    }
    db_leave();
}

//================================= Motor del turno ============================
// Combate modificado:
//  - A: siempre ataca si hay enemigos y tiene más probabilidad de ganar.
//  - B: nunca inicia combate, solo participa como defensora cuando la atacan.
//  - C: pacifista total, nunca inicia combate y nunca gana (si la atacan, muere).
void avanzar_turno(Partida *p) {
    size_t total = (size_t)p->filas * (size_t)p->columnas;
    int *nuevo      = (int*)malloc(total*sizeof(int));
    int *edad_nueva = (int*)malloc(total*sizeof(int));
    if (!nuevo || !edad_nueva) { if(nuevo)free(nuevo); if(edad_nueva)free(edad_nueva); return; }

    memcpy(nuevo, p->mundo, total*sizeof(int));
    if (p->edad) memcpy(edad_nueva, p->edad, total*sizeof(int));
    else memset(edad_nueva, 0, total*sizeof(int));

    const int dv[8][2]={{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};

    for (int i=0;i<p->filas;i++) for (int j=0;j<p->columnas;j++) {
        int idx = IDX(p,i,j);
        int especie = p->mundo[idx];
        if (especie==VACIO) continue;

        // 1) Muerte natural (depende de la especie)
        if ((rand()/(float)RAND_MAX) < longevidad_muerte[especie]) {
            nuevo[idx]=VACIO; edad_nueva[idx]=0; continue;
        }

        // 2) Escanear vecinos
        int same=0, empty_count=0;
        int enem_i[8], enem_j[8], n_enem=0;
        int vacios[8][2];

        for (int k=0;k<8;k++) {
            int ni=i+dv[k][0], nj=j+dv[k][1];
            if (ni<0||nj<0||ni>=p->filas||nj>=p->columnas) continue;
            int v=p->mundo[IDX(p,ni,nj)];
            if (v==VACIO) {
                if (empty_count<8){vacios[empty_count][0]=ni;vacios[empty_count][1]=nj;}
                empty_count++;
            } else if (v==especie) {
                same++;
            } else {
                if (n_enem<8){ enem_i[n_enem]=ni; enem_j[n_enem]=nj; }
                n_enem++;
            }
        }

        // 3) Combate según especie
        bool puede_atacar = false;
        if (especie == 1) {
            // A: siempre que vea otra especie, ataca
            puede_atacar = (n_enem > 0);
        } else if (especie == 2) {
            // B: nunca inicia combates
            puede_atacar = false;
        } else if (especie == 3) {
            // C: pacifista, nunca ataca
            puede_atacar = false;
        }

        if (puede_atacar && n_enem > 0) {
            // Elegir enemigo al azar
            int r = rand()%n_enem;
            int ei = enem_i[r], ej = enem_j[r];
            int eidx = IDX(p,ei,ej);
            int especie_rival = p->mundo[eidx];

            // Probabilidad de que la celda actual gane el combate
            float prob_ganar_actual = 0.5f;

            if (especie == 1) {
                // A agresiva: más probabilidad de ganar
                prob_ganar_actual = 0.7f;
            } else if (especie == 2) {
                prob_ganar_actual = 0.5f; // por si en algún futuro se le deja atacar
            } else if (especie == 3) {
                // C no mata a nadie
                prob_ganar_actual = 0.0f;
            }

            // C pacifista incluso como rival: nunca gana un combate
            // Si el rival es C, el atacante siempre gana
            if (especie_rival == 3) {
                prob_ganar_actual = 1.0f;
            }

            float r2 = rand()/(float)RAND_MAX;
            if (r2 < prob_ganar_actual) {
                // Gana el actual -> muere el rival
                nuevo[eidx] = VACIO;
                if (p->edad) edad_nueva[eidx]=0;
            } else {
                // Gana el rival -> muere el actual
                nuevo[idx] = VACIO; edad_nueva[idx]=0;
                continue;
            }
        }

        // 4) Reproducción con pareja y hueco
        if (same>0 && empty_count>0 && (rand()/(float)RAND_MAX) < fecundidad[especie]) {
            int pick = rand()%empty_count;
            int ci=vacios[pick][0], cj=vacios[pick][1];
            int cidx=IDX(p,ci,cj);
            if (nuevo[cidx]==VACIO) { nuevo[cidx]=especie; edad_nueva[cidx]=0; }
        }

        // 5) Envejecimiento
        edad_nueva[idx] = (nuevo[idx]!=VACIO) ? (edad_nueva[idx]+1) : 0;
    }

    free(p->mundo); if (p->edad) free(p->edad);
    p->mundo=nuevo; p->edad=edad_nueva;
}

//============================= Utilidades memoria =============================
void limpiarMundo(Partida *p) {
    size_t total=(size_t)p->filas*(size_t)p->columnas;
    for (size_t k=0;k<total;k++){ p->mundo[k]=VACIO; if(p->edad) p->edad[k]=0; }
}
void liberar_partida(Partida *p) {
    if (p) { free(p->mundo); free(p->edad); p->mundo=NULL; p->edad=NULL; p->filas=0; p->columnas=0; p->nombre[0]='\0'; }
}
void liberar_todas_las_partidas() {
    for (int i=0;i<n_partidas;i++) liberar_partida(&partidas[i]);
    n_partidas=0;
}

//=============================== Persistencia JSON ============================
static void json_escape_and_print(FILE* f, const char* s) {
    fputc('"', f);
    for (const char* p=s; *p; ++p) {
        unsigned char c=(unsigned char)*p;
        if (c=='\\'||c=='"'){ fputc('\\',f); fputc(c,f); }
        else if (c=='\n') fputs("\\n",f);
        else if (c=='\r') fputs("\\r",f);
        else if (c=='\t') fputs("\\t",f);
        else if (c<0x20)  fprintf(f,"\\u%04x",c);
        else fputc(c,f);
    }
    fputc('"', f);
}
int guardar_partidas_json(const char* path, Partida partidas[], int n) {
    FILE *f=fopen(path,"wb"); if(!f) return -1;
    fprintf(f, "{\n  \"version\": 1,\n  \"partidas\": [\n");
    for (int i=0;i<n;i++) {
        Partida *p=&partidas[i];
        if (i>0) fprintf(f,",\n");
        fprintf(f,"    {\n      \"nombre\": "); json_escape_and_print(f,p->nombre);
        fprintf(f,",\n      \"filas\": %d,\n      \"columnas\": %d,\n      \"mundo\": [\n        ", p->filas, p->columnas);
        size_t total=(size_t)p->filas*(size_t)p->columnas;
        for (size_t k=0;k<total;k++){
            fprintf(f,"%d", p->mundo[k]);
            if (k+1<total){ fputc(',',f); if((k+1)%p->columnas==0) fprintf(f,"\n        "); else fputc(' ',f); }
        }
        fprintf(f,"\n      ]\n    }");
    }
    fprintf(f,"\n  ]\n}\n"); fclose(f); return 0;
}

static char* read_file_all(const char* path, long* out_size) {
    FILE* f=fopen(path,"rb"); if(!f) return NULL;
    if (fseek(f,0,SEEK_END)!=0){ fclose(f); return NULL; }
    long sz=ftell(f); if (sz<0){ fclose(f); return NULL; }
    rewind(f);
    char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return NULL; }
    size_t rd=fread(buf,1,(size_t)sz,f); fclose(f); buf[rd]='\0'; if(out_size)*out_size=(long)rd; return buf;
}
static const char* skip_ws(const char* p){ while(*p && isspace((unsigned char)*p)) p++; return p; }

static int parse_string_field(const char* obj_start,const char* obj_end,const char* key,char* out,size_t outcap){
    char pattern[64]; snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    const char* k=strstr(obj_start,pattern); if(!k||k>=obj_end) return -1;
    const char* colon=strchr(k+strlen(pattern),':'); if(!colon||colon>=obj_end) return -1;
    const char* p=skip_ws(colon+1); if(*p!='"') return -1; p++;
    size_t idx=0;
    while(p<obj_end && *p && idx+1<outcap){
        if(*p=='\\'){ p++; if(!*p)break; char esc=*p++;
            switch(esc){ case '"':out[idx++]='"';break; case '\\':out[idx++]='\\';break;
            case 'n':out[idx++]='\n';break; case 'r':out[idx++]='\r';break; case 't':out[idx++]='\t';break;
            case 'u': for(int i=0;i<4 && *p;i++,p++); out[idx++]='?'; break; default: out[idx++]=esc; break; }
        } else if (*p=='"'){ p++; break; } else { out[idx++]=*p++; }
    }
    out[idx]='\0'; return 0;
}
static int parse_int_field(const char* obj_start,const char* obj_end,const char* key,int* out){
    char pattern[64]; snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    const char* k=strstr(obj_start,pattern); if(!k||k>=obj_end) return -1;
    const char* colon=strchr(k+strlen(pattern),':'); if(!colon||colon>=obj_end) return -1;
    const char* p=skip_ws(colon+1); char* endptr=NULL; long v=strtol(p,&endptr,10);
    if(p==endptr||endptr>obj_end) return -1; *out=(int)v; return 0;
}
static int parse_int_array_field(const char* obj_start,const char* obj_end,const char* key,int* out,size_t expected){
    char pattern[64]; snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    const char* k=strstr(obj_start,pattern); if(!k||k>=obj_end) return -1;
    const char* colon=strchr(k+strlen(pattern),':'); if(!colon||colon>=obj_end) return -1;
    const char* p=skip_ws(colon+1); if(*p!='[') return -1; p++;
    size_t count=0;
    while(p<obj_end){
        p=skip_ws(p); if(*p==']'){ p++; break; }
        char* endptr=NULL; long v=strtol(p,&endptr,10);
        if(p==endptr||endptr>obj_end) return -1;
        if(count<expected) out[count]=(int)v; count++;
        p=skip_ws(endptr); if(*p==','){ p++; continue; } else if(*p==']'){ p++; break; } else return -1;
    }
    if(count!=expected) return -1; return 0;
}
int cargar_partidas_json(const char* path, Partida partidas_out[], int *n_out) {
    long sz=0; char* text=read_file_all(path,&sz); if(!text) return -1;
    const char* start=text; const char* end=text+sz;
    const char* key="\"partidas\""; const char* k=strstr(start,key); if(!k){ free(text); return -1; }
    const char* colon=strchr(k+strlen(key),':'); if(!colon){ free(text); return -1; }
    const char* p=skip_ws(colon+1); if(*p!='['){ free(text); return -1; } p++;
    int count_loaded=0;

    while(p<end){
        p=skip_ws(p); if(*p==']'){ p++; break; }
        if(*p!='{'){ free(text); return -1; }
        const char* obj_start=p; int depth=0;
        while(p<end){ if(*p=='{') depth++; else if(*p=='}'){ depth--; if(depth==0){ p++; break; } } p++; }
        const char* obj_end=p;
        if(count_loaded>=MAX_PARTIDAS) break;

        Partida tmp; memset(&tmp,0,sizeof(tmp));
        if (parse_string_field(obj_start,obj_end,"nombre",tmp.nombre,sizeof(tmp.nombre))!=0){ free(text); return -1; }
        if (parse_int_field(obj_start,obj_end,"filas",&tmp.filas)!=0 ||
            parse_int_field(obj_start,obj_end,"columnas",&tmp.columnas)!=0){ free(text); return -1; }
        if (tmp.filas<MIN_FILAS||tmp.filas>MAX_FILAS||tmp.columnas<MIN_COLUMNAS||tmp.columnas>MAX_COLUMNAS){ free(text); return -1; }
        size_t total=(size_t)tmp.filas*(size_t)tmp.columnas;
        tmp.mundo=(int*)malloc(total*sizeof(int)); if(!tmp.mundo){ free(text); return -1; }
        if (parse_int_array_field(obj_start,obj_end,"mundo",tmp.mundo,total)!=0){ free(tmp.mundo); free(text); return -1; }

        partidas_out[count_loaded++]=tmp;
        p=skip_ws(p); if(*p==','){ p++; continue; } else if(*p==']'){ p++; break; }
    }
    *n_out=count_loaded; free(text); return 0;
}
