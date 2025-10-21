#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

#define ESPECIES 3
#define VACIO 0
#define MAX_PARTIDAS 5
#define MAX_NOMBRE 32
#define MIN_FILAS 5
#define MIN_COLUMNAS 5
#define MAX_FILAS 200
#define MAX_COLUMNAS 200

static const char* JSON_PATH = "partidas.json";
static const char* BIN_PATH  = "partidas.bin";

typedef struct {
    int filas;
    int columnas;
    int *mundo;
    char nombre[MAX_NOMBRE];
} Partida;

// Macros auxiliares
#define IDX(p,i,j) ((i) * (p)->columnas + (j))

// Prototipos
void avanzar_turno(Partida *p);
void menu_inicial();
void nueva_partida();
void cargar_partida();
void editar_mundo_con_mouse(Partida *p);
void simular_con_mouse(Partida *p);
void mostrarMundo(const Partida *p);
void limpiarMundo(Partida *p);
void liberar_partida(Partida *p);
void liberar_todas_las_partidas();
int leer_entero_en_rango(const char *prompt, int minv, int maxv);
int menu_simple(const char* titulo, const char* opciones[], int num_opciones);

// Persistencia JSON
int guardar_partidas_json(const char* path, Partida partidas[], int n);
int cargar_partidas_json(const char* path, Partida partidas[], int *n);
int cargar_partidas_bin_legacy(const char* path, Partida partidas[], int *n);

// Gestión de partidas
Partida partidas[MAX_PARTIDAS];
int n_partidas = 0;

// ============ Utilidades de consola para evitar parpadeo ============
static HANDLE g_hConsoleOut = NULL;

static void console_init(void) {
    g_hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    // Ocultar cursor para evitar parpadeo visual del caret
    CONSOLE_CURSOR_INFO cci;
    if (GetConsoleCursorInfo(g_hConsoleOut, &cci)) {
        cci.bVisible = FALSE;
        cci.dwSize = 100;
        SetConsoleCursorInfo(g_hConsoleOut, &cci);
    }
}

static void console_restore(void) {
    if (!g_hConsoleOut) return;
    CONSOLE_CURSOR_INFO cci;
    if (GetConsoleCursorInfo(g_hConsoleOut, &cci)) {
        cci.bVisible = TRUE;
        cci.dwSize = 20;
        SetConsoleCursorInfo(g_hConsoleOut, &cci);
    }
}

static void console_goto_xy(short x, short y) {
    if (!g_hConsoleOut) g_hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD c = { x, y };
    SetConsoleCursorPosition(g_hConsoleOut, c);
}

// Función de menú simple con selección numérica
int menu_simple(const char* titulo, const char* opciones[], int num_opciones) {
    int opcion;
    
    while (1) {
        system("cls");
        printf("=== %s ===\n\n", titulo);
        printf("Seleccione una opcion:\n\n");
        
        for (int i = 0; i < num_opciones; i++) {
            printf("%d. %s\n", i + 1, opciones[i]);
        }
        
        printf("\nOpcion: ");
        
        if (scanf("%d", &opcion) == 1) {
            getchar(); // Limpiar buffer
            if (opcion >= 1 && opcion <= num_opciones) {
                return opcion - 1;
            }
        } else {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
        }
        
        printf("\nOpcion invalida. Presione Enter para continuar...");
        getchar();
    }
}

int main() {
    srand((unsigned)time(NULL));
    console_init();
    menu_inicial();
    console_restore();
    liberar_todas_las_partidas();
    return 0;
}

void menu_inicial() {
    const char* opciones[] = {
        "Nueva Partida",
        "Cargar Partida", 
        "Salir"
    };
    
    int op;
    
    do {
        op = menu_simple("SIMULADOR DE VIDA", opciones, 3);
        
        switch (op) {
            case 0:
                nueva_partida();
                break;
            case 1:
                cargar_partida();
                break;
            case 2:
                printf("Adios!\n");
                Sleep(1000);
                break;
        }
    } while (op != 2);
}

void nueva_partida() {
    if (n_partidas >= MAX_PARTIDAS) {
        system("cls");
        printf("Maximo de partidas alcanzado.\n");
        Sleep(2000);
        return;
    }

    Partida *p = &partidas[n_partidas];
    
    // Configurar tamaño
    const char* tamanos[] = {
        "Pequeño (10x10)",
        "Mediano (25x25)", 
        "Grande (50x50)",
        "Muy Grande (100x100)",
        "Personalizado",
        "Volver"
    };
    
    int size_choice = menu_simple("CONFIGURAR TAMAÑO DEL MUNDO", tamanos, 6);
    
    switch (size_choice) {
        case 0: p->filas = 10; p->columnas = 10; break;
        case 1: p->filas = 25; p->columnas = 25; break;
        case 2: p->filas = 50; p->columnas = 50; break;
        case 3: p->filas = 100; p->columnas = 100; break;
        case 4: 
            system("cls");
            p->filas = leer_entero_en_rango("Filas (5-200): ", MIN_FILAS, MAX_FILAS);
            p->columnas = leer_entero_en_rango("Columnas (5-200): ", MIN_COLUMNAS, MAX_COLUMNAS);
            break;
        case 5: return; // Volver
    }

    size_t total = (size_t)p->filas * (size_t)p->columnas;
    p->mundo = (int*)malloc(total * sizeof(int));
    if (!p->mundo) {
        printf("Error: no se pudo reservar memoria.\n");
        Sleep(1200);
        return;
    }

    // Nombre de la partida
    system("cls");
    printf("Nombre de la partida (max 31 caracteres): ");
    if (!fgets(p->nombre, MAX_NOMBRE, stdin)) {
        strcpy(p->nombre, "Partida");
    }
    p->nombre[strcspn(p->nombre, "\n")] = 0;

    limpiarMundo(p);
    editar_mundo_con_mouse(p);
    simular_con_mouse(p);

    n_partidas++;
    if (guardar_partidas_json(JSON_PATH, partidas, n_partidas) != 0) {
        printf("No se pudo guardar en JSON.\n");
        Sleep(1200);
    }
}

void cargar_partida() {
    liberar_todas_las_partidas();

    int res = cargar_partidas_json(JSON_PATH, partidas, &n_partidas);
    if (res != 0) {
        res = cargar_partidas_bin_legacy(BIN_PATH, partidas, &n_partidas);
        if (res != 0) {
            system("cls");
            printf("No hay partidas guardadas.\n");
            Sleep(2000);
            return;
        }
    }

    if (n_partidas == 0) {
        system("cls");
        printf("No hay partidas.\n");
        Sleep(2000);
        return;
    }

    // Mostrar partidas disponibles
    system("cls");
    printf("Partidas disponibles:\n\n");
    
    for (int i = 0; i < n_partidas; i++) {
        printf("%d. %s (%dx%d)\n", i + 1, partidas[i].nombre, partidas[i].filas, partidas[i].columnas);
    }
    printf("%d. Volver\n", n_partidas + 1);
    
    int choice = leer_entero_en_rango("\nSeleccione el numero de la partida: ", 1, n_partidas + 1);
    
    if (choice == n_partidas + 1) return; // Volver
    
    simular_con_mouse(&partidas[choice - 1]);

    if (guardar_partidas_json(JSON_PATH, partidas, n_partidas) != 0) {
        printf("No se pudo guardar en JSON.\n");
        Sleep(1200);
    }
}

void editar_mundo_con_mouse(Partida *p) {
    int especie_actual = 1;
    int editing = 1;
    
    system("cls");
    printf("\n=== MODO EDICION ===\n");
    printf("Instrucciones:\n");
    printf("- Use las teclas WASD para mover el cursor\n");
    printf("- Teclas 1,2,3: Colocar especies A,B,C\n");
    printf("- Tecla 0: Eliminar especie\n");
    printf("- Tecla L: Limpiar todo el mundo\n");
    printf("- Tecla ESC: Terminar edicion\n\n");
    printf("Presione cualquier tecla para continuar...");
    getchar();

    int cursor_x = 0, cursor_y = 0;

    system("cls");
    while (editing) {
        console_goto_xy(0, 0);
        printf("Edicion del mundo '%s' (%dx%d)\n", p->nombre, p->filas, p->columnas);
        printf("Especie actual: %c | Cursor: (%d,%d)\n\n", 'A' + especie_actual - 1, cursor_x, cursor_y);
        
        // Mostrar mundo con cursor
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        for (int i = 0; i < p->filas; i++) {
            for (int j = 0; j < p->columnas; j++) {
                int val = p->mundo[IDX(p, i, j)];
                
                // Resaltar cursor
                if (i == cursor_x && j == cursor_y) {
                    SetConsoleTextAttribute(hConsole, 224); // Fondo amarillo
                }
                
                if (val == VACIO) {
                    printf(". ");
                }
                else if (val == 1) {
                    printf("A ");
                }
                else if (val == 2) {
                    printf("B ");
                }
                else if (val == 3) {
                    printf("C ");
                } else {
                    printf("? ");
                }
                
                SetConsoleTextAttribute(hConsole, 7);
            }
            printf("\n");
        }
        
        printf("\nControles: WASD-mover, 1/2/3-colocar, 0-eliminar, L-limpiar, ESC-salir\n");

        // Leer tecla (bloqueante para evitar redibujos innecesarios)
        int tecla = _getch();
        switch (tecla) {
            case 'w': case 'W': if (cursor_x > 0) cursor_x--; break;
            case 's': case 'S': if (cursor_x < p->filas - 1) cursor_x++; break;
            case 'a': case 'A': if (cursor_y > 0) cursor_y--; break;
            case 'd': case 'D': if (cursor_y < p->columnas - 1) cursor_y++; break;
            case '1': especie_actual = 1; p->mundo[IDX(p, cursor_x, cursor_y)] = 1; break;
            case '2': especie_actual = 2; p->mundo[IDX(p, cursor_x, cursor_y)] = 2; break;
            case '3': especie_actual = 3; p->mundo[IDX(p, cursor_x, cursor_y)] = 3; break;
            case '0': p->mundo[IDX(p, cursor_x, cursor_y)] = VACIO; break;
            case 'l': case 'L': limpiarMundo(p); break;
            case 27: editing = 0; break; // ESC
        }
    }
}

void simular_con_mouse(Partida *p) {
    int running = 1;
    
    system("cls");
    printf("\n=== MODO SIMULACION ===\n");
    printf("Controles:\n");
    printf("- Tecla 1: Avanzar 1 turno\n");
    printf("- Tecla 2: Avanzar 10 turnos\n");
    printf("- Tecla N: Avanzar N turnos\n");
    printf("- Tecla 3: Editar mundo\n");
    printf("- Tecla 4: Guardar y salir\n\n");
    printf("Presione cualquier tecla para continuar...");
    getchar();
    
    system("cls");
    while (running) {
        console_goto_xy(0, 0);
        printf("Simulando partida '%s' (%dx%d)\n\n", p->nombre, p->filas, p->columnas);
        mostrarMundo(p);
        printf("\nControles: 1-1 turno, 2-10 turnos, N-N turnos, 3-editar, 4-salir\n");

        // Leer tecla (bloqueante para no redibujar si no hay input)
        int tecla = _getch();
        switch (tecla) {
            case '1': // Avanzar 1 turno
                avanzar_turno(p);
                break;
            case '2': // Avanzar 10 turnos (sin render intermedio)
                for (int t = 0; t < 10; t++) {
                    avanzar_turno(p);
                }
                break;
            case 'n': case 'N': { // Avanzar N turnos (sin render intermedio)
                int n = leer_entero_en_rango("Cuantos turnos avanzar?: ", 1, 100000000);
                for (int t = 0; t < n; t++) {
                    avanzar_turno(p);
                }
                break;
            }
            case '3': // Editar mundo
                editar_mundo_con_mouse(p);
                system("cls");
                break;
            case '4': // Guardar y salir
                running = 0;
                break;
        }
        // El siguiente ciclo redibuja el mundo ya con el cursor en (0,0)
    }
}

void avanzar_turno(Partida *p) {
    size_t total = (size_t)p->filas * (size_t)p->columnas;
    int *nuevo = (int*)malloc(total * sizeof(int));
    if (!nuevo) return;
    
    memcpy(nuevo, p->mundo, total * sizeof(int));

    for (int i = 0; i < p->filas; i++) {
        for (int j = 0; j < p->columnas; j++) {
            int especie = p->mundo[IDX(p, i, j)];
            if (especie != VACIO) {
                if (rand() % 10 == 0) {
                    nuevo[IDX(p, i, j)] = VACIO;
                    continue;
                }
                int pareja = 0;
                for (int di = -1; di <= 1 && !pareja; di++) {
                    for (int dj = -1; dj <= 1 && !pareja; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di, nj = j + dj;
                        if (ni >= 0 && ni < p->filas && nj >= 0 && nj < p->columnas) {
                            if (p->mundo[IDX(p, ni, nj)] == especie) pareja = 1;
                        }
                    }
                }
                if (pareja) {
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            if (di == 0 && dj == 0) continue;
                            int ni = i + di, nj = j + dj;
                            if (ni >= 0 && ni < p->filas && nj >= 0 && nj < p->columnas) {
                                if (p->mundo[IDX(p, ni, nj)] == VACIO && rand() % 10 == 0) {
                                    nuevo[IDX(p, ni, nj)] = especie;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    free(p->mundo);
    p->mundo = nuevo;
}

void mostrarMundo(const Partida *p) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    for (int i = 0; i < p->filas; i++) {
        for (int j = 0; j < p->columnas; j++) {
            int val = p->mundo[IDX(p, i, j)];
            if (val == VACIO) {
                SetConsoleTextAttribute(hConsole, 8);
                printf(". ");
            }
            else if (val == 1) {
                SetConsoleTextAttribute(hConsole, 10);
                printf("A ");
            }
            else if (val == 2) {
                SetConsoleTextAttribute(hConsole, 12);
                printf("B ");
            }
            else if (val == 3) {
                SetConsoleTextAttribute(hConsole, 11);
                printf("C ");
            } else {
                SetConsoleTextAttribute(hConsole, 7);
                printf("? ");
            }
        }
        printf("\n");
    }
    SetConsoleTextAttribute(hConsole, 7);
}

void limpiarMundo(Partida *p) {
    size_t total = (size_t)p->filas * (size_t)p->columnas;
    for (size_t k = 0; k < total; k++)
        p->mundo[k] = VACIO;
}

void liberar_partida(Partida *p) {
    if (p && p->mundo) {
        free(p->mundo);
        p->mundo = NULL;
    }
    p->filas = 0;
    p->columnas = 0;
    p->nombre[0] = '\0';
}

void liberar_todas_las_partidas() {
    for (int i = 0; i < n_partidas; i++) {
        liberar_partida(&partidas[i]);
    }
    n_partidas = 0;
}

int leer_entero_en_rango(const char *prompt, int minv, int maxv) {
    int v;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d", &v) == 1) {
            getchar();
            if (v >= minv && v <= maxv) return v;
        } else {
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
        }
        printf("Valor invalido. Debe estar entre %d y %d.\n", minv, maxv);
    }
}

// ====================== PERSISTENCIA JSON ======================

static void json_escape_and_print(FILE* f, const char* s) {
    fputc('"', f);
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == '"') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c == '\n') {
            fputs("\\n", f);
        } else if (c == '\r') {
            fputs("\\r", f);
        } else if (c == '\t') {
            fputs("\\t", f);
        } else if (c < 0x20) {
            fprintf(f, "\\u%04x", c);
        } else {
            fputc(c, f);
        }
    }
    fputc('"', f);
}

int guardar_partidas_json(const char* path, Partida partidas[], int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fprintf(f, "{\n  \"version\": 1,\n  \"partidas\": [\n");

    for (int i = 0; i < n; i++) {
        Partida *p = &partidas[i];
        if (i > 0) fprintf(f, ",\n");
        fprintf(f, "    {\n      \"nombre\": ");
        json_escape_and_print(f, p->nombre);
        fprintf(f, ",\n      \"filas\": %d,\n      \"columnas\": %d,\n      \"mundo\": [\n        ", p->filas, p->columnas);

        size_t total = (size_t)p->filas * (size_t)p->columnas;
        for (size_t k = 0; k < total; k++) {
            fprintf(f, "%d", p->mundo[k]);
            if (k + 1 < total) {
                fputc(',', f);
                if ((k + 1) % p->columnas == 0) {
                    fprintf(f, "\n        ");
                } else {
                    fputc(' ', f);
                }
            }
        }
        fprintf(f, "\n      ]\n    }");
    }

    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    return 0;
}

static char* read_file_all(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_size) *out_size = (long)rd;
    return buf;
}

static const char* skip_ws(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int parse_string_field(const char* obj_start, const char* obj_end, const char* key, char* out, size_t outcap) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* k = strstr(obj_start, pattern);
    if (!k || k >= obj_end) return -1;
    const char* colon = strchr(k + strlen(pattern), ':');
    if (!colon || colon >= obj_end) return -1;
    const char* p = skip_ws(colon + 1);
    if (*p != '"') return -1;
    p++;
    size_t idx = 0;
    while (p < obj_end && *p && idx + 1 < outcap) {
        if (*p == '\\') {
            p++;
            if (!*p) break;
            char esc = *p++;
            switch (esc) {
                case '"': out[idx++] = '"'; break;
                case '\\': out[idx++] = '\\'; break;
                case 'n': out[idx++] = '\n'; break;
                case 'r': out[idx++] = '\r'; break;
                case 't': out[idx++] = '\t'; break;
                case 'u':
                    for (int i = 0; i < 4 && *p; i++, p++);
                    out[idx++] = '?';
                    break;
                default:
                    out[idx++] = esc; break;
            }
        } else if (*p == '"') {
            p++; break;
        } else {
            out[idx++] = *p++;
        }
    }
    out[idx] = '\0';
    return 0;
}

static int parse_int_field(const char* obj_start, const char* obj_end, const char* key, int* out) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* k = strstr(obj_start, pattern);
    if (!k || k >= obj_end) return -1;
    const char* colon = strchr(k + strlen(pattern), ':');
    if (!colon || colon >= obj_end) return -1;
    const char* p = skip_ws(colon + 1);
    char* endptr = NULL;
    long v = strtol(p, &endptr, 10);
    if (p == endptr || endptr > obj_end) return -1;
    *out = (int)v;
    return 0;
}

static int parse_int_array_field(const char* obj_start, const char* obj_end, const char* key, int* out, size_t expected) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* k = strstr(obj_start, pattern);
    if (!k || k >= obj_end) return -1;
    const char* colon = strchr(k + strlen(pattern), ':');
    if (!colon || colon >= obj_end) return -1;
    const char* p = skip_ws(colon + 1);
    if (*p != '[') return -1;
    p++;
    size_t count = 0;
    while (p < obj_end) {
        p = skip_ws(p);
        if (*p == ']') { p++; break; }
        char* endptr = NULL;
        long v = strtol(p, &endptr, 10);
        if (p == endptr || endptr > obj_end) return -1;
        if (count < expected) out[count] = (int)v;
        count++;
        p = skip_ws(endptr);
        if (*p == ',') { p++; continue; }
        else if (*p == ']') { p++; break; }
        else return -1;
    }
    if (count != expected) return -1;
    return 0;
}

int cargar_partidas_json(const char* path, Partida partidas_out[], int *n_out) {
    long sz = 0;
    char* text = read_file_all(path, &sz);
    if (!text) return -1;

    const char* start = text;
    const char* end = text + sz;

    const char* key = "\"partidas\"";
    const char* k = strstr(start, key);
    if (!k) { free(text); return -1; }
    const char* colon = strchr(k + strlen(key), ':');
    if (!colon) { free(text); return -1; }
    const char* p = skip_ws(colon + 1);
    if (*p != '[') { free(text); return -1; }
    p++;
    int count_loaded = 0;

    while (p < end) {
        p = skip_ws(p);
        if (*p == ']') { p++; break; }
        if (*p != '{') { free(text); return -1; }
        const char* obj_start = p;
        int depth = 0;
        while (p < end) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) { p++; break; }
            }
            p++;
        }
        const char* obj_end = p;
        if (count_loaded >= MAX_PARTIDAS) break;

        Partida tmp;
        memset(&tmp, 0, sizeof(tmp));

        if (parse_string_field(obj_start, obj_end, "nombre", tmp.nombre, sizeof(tmp.nombre)) != 0) {
            free(text); return -1;
        }
        if (parse_int_field(obj_start, obj_end, "filas", &tmp.filas) != 0 ||
            parse_int_field(obj_start, obj_end, "columnas", &tmp.columnas) != 0) {
            free(text); return -1;
        }
        if (tmp.filas < MIN_FILAS || tmp.filas > MAX_FILAS ||
            tmp.columnas < MIN_COLUMNAS || tmp.columnas > MAX_COLUMNAS) {
            free(text); return -1;
        }
        size_t total = (size_t)tmp.filas * (size_t)tmp.columnas;
        tmp.mundo = (int*)malloc(total * sizeof(int));
        if (!tmp.mundo) { free(text); return -1; }

        if (parse_int_array_field(obj_start, obj_end, "mundo", tmp.mundo, total) != 0) {
            free(tmp.mundo);
            free(text);
            return -1;
        }

        partidas_out[count_loaded++] = tmp;

        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        else if (*p == ']') { p++; break; }
    }

    *n_out = count_loaded;
    free(text);
    return 0;
}

int cargar_partidas_bin_legacy(const char* path, Partida partidas_out[], int *n_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    int n = 0;
    if (fread(&n, sizeof(int), 1, f) != 1) {
        fclose(f); return -1;
    }
    if (n < 0 || n > MAX_PARTIDAS) {
        fclose(f); return -1;
    }

    for (int i = 0; i < n; i++) {
        Partida p;
        memset(&p, 0, sizeof(p));
        if (fread(&p.filas, sizeof(int), 1, f) != 1 ||
            fread(&p.columnas, sizeof(int), 1, f) != 1) {
            fclose(f); return -1;
        }
        if (p.filas < MIN_FILAS || p.filas > MAX_FILAS ||
            p.columnas < MIN_COLUMNAS || p.columnas > MAX_COLUMNAS) {
            fclose(f); return -1;
        }
        if (fread(p.nombre, sizeof(char), MAX_NOMBRE, f) != MAX_NOMBRE) {
            fclose(f); return -1;
        }
        p.nombre[MAX_NOMBRE - 1] = '\0';

        size_t total = (size_t)p.filas * (size_t)p.columnas;
        p.mundo = (int*)malloc(total * sizeof(int));
        if (!p.mundo) { fclose(f); return -1; }
        if (fread(p.mundo, sizeof(int), total, f) != total) {
            free(p.mundo); fclose(f); return -1;
        }

        partidas_out[i] = p;
    }

    *n_out = n;
    fclose(f);
    return 0;
}