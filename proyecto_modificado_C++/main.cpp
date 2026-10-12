#include <iostream>
#include <cstdlib>   // para datos al azar xd
#include <ctime>     // devolución del tiempo

using namespace std;                 // Para escribir cout en vez de std::cout

// Class Mundo - the board of the simulation
class Mundo
{
  private:
    int filas;                       // Número de filas del tablero
    int columnas;                    // Número de columnas del tablero
    int * celdas;                    // Puntero al arreglo dinámico: 0 = vacío, 1/2/3 = especie

  public:
    // Constructor                   // Se ejecuta solo al crear un Mundo
    Mundo( int f, int c )
    {
      filas = f;                     // Guarda las filas recibidas
      columnas = c;                  // Guarda las columnas recibidas
      celdas = new int[ filas * columnas ];    // Pide al heap filas*columnas enteros y guarda su dirección
      for( int k = 0; k < filas * columnas; k++ )    // Recorre todas las posiciones del arreglo
        celdas[ k ] = 0;             // Cada celda empieza vacía
    }
    // Destructor                    // Se ejecuta solo cuando el objeto desaparece
    ~Mundo( )
    { delete [ ] celdas; }           // Devuelve la memoria al heap (con [ ] porque es un arreglo)
    // Returns what is in cell (i, j)
    int obtenerCelda( int i, int j ) const    // const: promete no modificar el objeto
    { return celdas[ i * columnas + j ]; }    // Convierte (fila, columna) en una posición del arreglo

    void ponerCelda( int i, int j, int especie )
    { celdas[ i * columnas + j ] = especie; } // Guarda la especie en esa posición
    // Prints the world on the screen
    void mostrar( ) const
    {
      for( int i = 0; i < filas; i++ )        // Recorre las filas, de arriba hacia abajo
      {
        for( int j = 0; j < columnas; j++ )   // Recorre las columnas de esa fila
        {
          int valor = obtenerCelda( i, j );   // Lee qué hay en la celda (i, j)
          if( valor == 0 )
            cout << ". ";                     // Vacía: punto
          else if( valor == 1 )
            cout << "A ";                     // Especie 1
          else if( valor == 2 )
            cout << "B ";                     // Especie 2
          else
            cout << "C ";                     // Especie 3
        }
        cout << endl;                         // Al terminar la fila, salta de línea
      }
    }
        // Counts how many neighbors of a species are around cell (i, j)
    int contarVecinos( int i, int j, int especie ) const
    {
      int cuenta = 0;                        // Contador de vecinos de esa especie
      for( int di = -1; di <= 1; di++ )      // Desplazamiento en filas: -1, 0, 1
      {
        for( int dj = -1; dj <= 1; dj++ )    // Desplazamiento en columnas: -1, 0, 1
        {
          if( di == 0 && dj == 0 )           // Esa es la propia celda, no cuenta
            continue;
          int ni = i + di;                   // Fila del vecino
          int nj = j + dj;                   // Columna del vecino
          if( ni < 0 || nj < 0 || ni >= filas || nj >= columnas )  // Fuera del tablero
            continue;
          if( obtenerCelda( ni, nj ) == especie )
            cuenta++;                        // Vecino de la misma especie
        }
      }
      return cuenta;
    }
        // Advances the world one turn: individuals with a partner reproduce
    void avanzarTurno( )
    {
      int total = filas * columnas;
      int * nuevo = new int[ total ];             // Copia del mundo donde escribimos los cambios
      for( int k = 0; k < total; k++ )
        nuevo[ k ] = celdas[ k ];                 // La copia empieza igual al mundo actual
      for( int i = 0; i < filas; i++ )
      {
        for( int j = 0; j < columnas; j++ )
        {
          int especie = obtenerCelda( i, j );     // Qué individuo hay aquí
          if( especie == 0 )
            continue;                               // Celda vacía: nada que hacer
          if( rand( ) % 100 < probMuerte( especie ) )   // ¿Muere de forma natural?
          {
            nuevo[ i * columnas + j ] = 0;              // Se borra de la copia
            continue;                                   // Un muerto no se reproduce
          }

          if( contarVecinos( i, j, especie ) == 0 )
            continue;                             // Sin pareja no se reproduce
          if( rand( ) % 100 >= 20 )
            continue;                             // Solo 20% de probabilidad por turno
          int vi[ 8 ];                            // Filas de los huecos libres
          int vj[ 8 ];                            // Columnas de los huecos libres
          int libres = 0;                         // Cantidad de huecos libres
          for( int di = -1; di <= 1; di++ )
          {
            for( int dj = -1; dj <= 1; dj++ )
            {
              if( di == 0 && dj == 0 )
                continue;
              int ni = i + di;
              int nj = j + dj;
              if( ni < 0 || nj < 0 || ni >= filas || nj >= columnas )
                continue;
              if( nuevo[ ni * columnas + nj ] == 0 )   // Hueco libre en la copia
              {
                vi[ libres ] = ni;
                vj[ libres ] = nj;
                libres++;
              }
            }
          }
          if( libres == 0 )
            continue;                             // No hay donde poner al hijo
          int elegido = rand( ) % libres;         // Escoge un hueco al azar
          nuevo[ vi[ elegido ] * columnas + vj[ elegido ] ] = especie;   // Nace el hijo
        }
      }
      delete [ ] celdas;                          // Libera el arreglo viejo
      celdas = nuevo;                             // Ahora el mundo apunta a la copia nueva
    }
     int probMuerte( int especie ) const
    {
      if( especie == 1 )
        return 7;                      // A: la más mortal
      else if( especie == 2 )
        return 4;                      // B: normal
      else
        return 2;                      // C: la que más vive
    }
};                                   // cierre de la clase

// Main function
int main( )
{
  srand( time( 0 ) );                // Semilla: hace que el azar cambie en cada ejecución
  Mundo m( 5, 5 );
  m.ponerCelda( 1, 1, 1 );
  m.ponerCelda( 1, 2, 1 );           // Pareja de A
  m.ponerCelda( 3, 3, 2 );
  m.ponerCelda( 3, 4, 2 );           // Pareja de B
  m.ponerCelda( 0, 0, 3 );
  m.ponerCelda( 0, 1, 3 );           // Pareja de C
  for( int t = 0; t < 12; t++ )
  {
    cout << "Turno " << t << endl;
    m.mostrar( );
    m.avanzarTurno( );
  }
  return 0;
}
