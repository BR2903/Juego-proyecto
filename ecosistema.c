#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <string.h>

#define FILAS 20
#define COLUMNAS 40
#define ESPECIES 3
#define VACIO 0
#define MAX_PARTIDAS 5
#define MAX_NOMBRE 32

typedef struct {
    int mundo[FILAS][COLUMNAS];
    char nombre[MAX_NOMBRE];
} Partida;

// Prototipos
void menu_inicial();
void nueva_partida();
void cargar_partida();
void guardar_partida(Partida partidas[], int n);
void editar_mundo(Partida *p);
void simular(Partida *p);
void mostrarMundo(int mundo[FILAS][COLUMNAS]);
void limpiarMundo(int mundo[FILAS][COLUMNAS]);

// Gestión de partidas
Partida partidas[MAX_PARTIDAS];
int n_partidas = 0;

int main() {
    menu_inicial();
    return 0;
}

void menu_inicial() {
    int op;
    do {
        system("cls");
        printf("=== SIMULADOR DE VIDA ===\n");
        printf("1. Nueva partida\n");
        printf("2. Cargar partida\n");
        printf("3. Salir\n");
        printf("Seleccione una opcion: ");
        scanf("%d", &op);
        getchar(); // Limpiar buffer

        switch (op) {
            case 1:
                nueva_partida();
                break;
            case 2:
                cargar_partida();
                break;
            case 3:
                printf("Adios!\n");
                break;
            default:
                printf("Opcion invalida\n");
                Sleep(1000);
        }
    } while (op != 3);
}

void nueva_partida() {
    if (n_partidas >= MAX_PARTIDAS) {
        printf("Maximo de partidas alcanzado.\n");
        Sleep(1000);
        return;
    }
    Partida p;
    limpiarMundo(p.mundo);
    printf("Nombre de la partida: ");
    fgets(p.nombre, MAX_NOMBRE, stdin);
    p.nombre[strcspn(p.nombre, "\n")] = 0; // Quitar salto de línea
    editar_mundo(&p);
    simular(&p);
    partidas[n_partidas++] = p;
    guardar_partida(partidas, n_partidas);
}

void cargar_partida() {
    FILE *f = fopen("partidas.bin", "rb");
    if (!f) {
        printf("No hay partidas guardadas.\n");
        Sleep(1000);
        return;
    }
    fread(&n_partidas, sizeof(int), 1, f);
    fread(partidas, sizeof(Partida), n_partidas, f);
    fclose(f);

    printf("Partidas disponibles:\n");
    for (int i = 0; i < n_partidas; i++)
        printf("%d. %s\n", i + 1, partidas[i].nombre);

    int n;
    printf("Seleccione el numero de la partida: ");
    scanf("%d", &n);
    getchar();
    if (n < 1 || n > n_partidas) {
        printf("Opcion invalida.\n");
        Sleep(1000);
        return;
    }
    simular(&partidas[n-1]);
}

void guardar_partida(Partida partidas[], int n) {
    FILE *f = fopen("partidas.bin", "wb");
    if (!f) return;
    fwrite(&n, sizeof(int), 1, f);
    fwrite(partidas, sizeof(Partida), n, f);
    fclose(f);
}

// Panel de edición del mundo
void editar_mundo(Partida *p) {
    int x, y, especie, op;
    do {
        system("cls");
        printf("Edicion del mundo '%s'\n", p->nombre);
        mostrarMundo(p->mundo);
        printf("\n1. Agregar especie\n2. Eliminar especie\n3. Terminar edicion\n");
        printf("Elija opcion: ");
        scanf("%d", &op);
        getchar();
        if (op == 1) {
            printf("Coordenada X (0-%d): ", FILAS-1); scanf("%d", &x);
            printf("Coordenada Y (0-%d): ", COLUMNAS-1); scanf("%d", &y);
            printf("Especie (1=A, 2=B, 3=C): "); scanf("%d", &especie);
            getchar();
            if (x >= 0 && x < FILAS && y >= 0 && y < COLUMNAS && especie >= 1 && especie <= ESPECIES)
                p->mundo[x][y] = especie;
        } else if (op == 2) {
            printf("Coordenada X (0-%d): ", FILAS-1); scanf("%d", &x);
            printf("Coordenada Y (0-%d): ", COLUMNAS-1); scanf("%d", &y);
            getchar();
            if (x >= 0 && x < FILAS && y >= 0 && y < COLUMNAS)
                p->mundo[x][y] = VACIO;
        }
    } while (op != 3);
}

void limpiarMundo(int mundo[FILAS][COLUMNAS]) {
    for (int i = 0; i < FILAS; i++)
        for (int j = 0; j < COLUMNAS; j++)
            mundo[i][j] = VACIO;
}

// Lógica de simulación
void simular(Partida *p) {
    int op, turnos = 1;
    do {
        system("cls");
        printf("Simulando partida '%s'\n", p->nombre);
        mostrarMundo(p->mundo);
        printf("\n1. Avanzar 1 turno\n2. Avanzar N turnos\n3. Editar mundo (modo Dios)\n4. Guardar y salir\n");
        printf("Elija opcion: ");
        scanf("%d", &op);
        getchar();
        if (op == 1) {
            for (int t = 0; t < 1; t++) {
                // Simular un turno
                int nuevo[FILAS][COLUMNAS];
                memcpy(nuevo, p->mundo, sizeof(nuevo));

                for (int i = 0; i < FILAS; i++) {
                    for (int j = 0; j < COLUMNAS; j++) {
                        int especie = p->mundo[i][j];
                        if (especie != VACIO) {
                            // Muerte aleatoria
                            if (rand() % 10 == 0) { // 10% de morir
                                nuevo[i][j] = VACIO;
                                continue;
                            }
                            // Reproducción: necesita pareja vecina de misma especie
                            int pareja = 0;
                            for (int di = -1; di <= 1; di++) {
                                for (int dj = -1; dj <= 1; dj++) {
                                    if (di == 0 && dj == 0) continue;
                                    int ni = i + di, nj = j + dj;
                                    if (ni >= 0 && ni < FILAS && nj >= 0 && nj < COLUMNAS)
                                        if (p->mundo[ni][nj] == especie) pareja = 1;
                                }
                            }
                            if (pareja) {
                                // Solo si hay celda vacía vecina
                                for (int di = -1; di <= 1; di++) {
                                    for (int dj = -1; dj <= 1; dj++) {
                                        if (di == 0 && dj == 0) continue;
                                        int ni = i + di, nj = j + dj;
                                        if (ni >= 0 && ni < FILAS && nj >= 0 && nj < COLUMNAS)
                                            if (p->mundo[ni][nj] == VACIO && rand() % 10 == 0)
                                                nuevo[ni][nj] = especie; // probabilidad baja de reproducirse
                                    }
                                }
                            }
                        }
                    }
                }
                memcpy(p->mundo, nuevo, sizeof(p->mundo));
            }
        } else if (op == 2) {
            printf("Cuantos turnos quieres avanzar? ");
            scanf("%d", &turnos);
            getchar();
            for (int t = 0; t < turnos; t++) {
                // Repetimos el mismo avance que arriba
                int nuevo[FILAS][COLUMNAS];
                memcpy(nuevo, p->mundo, sizeof(nuevo));
                for (int i = 0; i < FILAS; i++) {
                    for (int j = 0; j < COLUMNAS; j++) {
                        int especie = p->mundo[i][j];
                        if (especie != VACIO) {
                            if (rand() % 10 == 0) {
                                nuevo[i][j] = VACIO;
                                continue;
                            }
                            int pareja = 0;
                            for (int di = -1; di <= 1; di++) {
                                for (int dj = -1; dj <= 1; dj++) {
                                    if (di == 0 && dj == 0) continue;
                                    int ni = i + di, nj = j + dj;
                                    if (ni >= 0 && ni < FILAS && nj >= 0 && nj < COLUMNAS)
                                        if (p->mundo[ni][nj] == especie) pareja = 1;
                                }
                            }
                            if (pareja) {
                                for (int di = -1; di <= 1; di++) {
                                    for (int dj = -1; dj <= 1; dj++) {
                                        if (di == 0 && dj == 0) continue;
                                        int ni = i + di, nj = j + dj;
                                        if (ni >= 0 && ni < FILAS && nj >= 0 && nj < COLUMNAS)
                                            if (p->mundo[ni][nj] == VACIO && rand() % 10 == 0)
                                                nuevo[ni][nj] = especie;
                                    }
                                }
                            }
                        }
                    }
                }
                memcpy(p->mundo, nuevo, sizeof(p->mundo));
            }
        } else if (op == 3) {
            editar_mundo(p);
        }
    } while (op != 4);
    guardar_partida(partidas, n_partidas);
}

// Mostrar mundo con colores
void mostrarMundo(int mundo[FILAS][COLUMNAS]) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLUMNAS; j++) {
            if (mundo[i][j] == VACIO) {
                SetConsoleTextAttribute(hConsole, 8);  // Gris
                printf(".");
            }
            else if (mundo[i][j] == 1) {
                SetConsoleTextAttribute(hConsole, 10); // Verde claro
                printf("A");
            }
            else if (mundo[i][j] == 2) {
                SetConsoleTextAttribute(hConsole, 12); // Rojo claro
                printf("B");
            }
            else if (mundo[i][j] == 3) {
                SetConsoleTextAttribute(hConsole, 11); // Aqua/celeste claro
                printf("C");
            }
        }
        printf("\n");
    }
    SetConsoleTextAttribute(hConsole, 7); // Restaurar color estándar
}