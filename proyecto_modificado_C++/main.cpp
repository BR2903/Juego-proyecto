#include <iostream>
#include <cstdlib>   // para datos al azar xd
#include <ctime>     // devolucion del tiempo
#include <iomanip>   // [NUEVO] setw para alinear el tablero con indices
#include <limits>    // [NUEVO] limpiar el buffer de cin en lecturas invalidas
#include <string>    // [NUEVO] prompts de las funciones de lectura

using namespace std;                 // Para escribir cout en vez de std::cout

// Class Mundo - the board of the simulation
class Mundo
{
  private:
    int filas;                       // Numero de filas del tablero
    int columnas;                    // Numero de columnas del tablero
    int * celdas;                    // Puntero al arreglo dinamico: 0 = vacio, 1/2/3 = especie

  public:
    // Constructor                   // Se ejecuta solo al crear un Mundo
    Mundo( int f, int c )
    {
      filas = f;                     // Guarda las filas recibidas
      columnas = c;                  // Guarda las columnas recibidas
      celdas = new int[ filas * columnas ];    // Pide al heap filas*columnas enteros y guarda su direccion
      for( int k = 0; k < filas * columnas; k++ )    // Recorre todas las posiciones del arreglo
        celdas[ k ] = 0;             // Cada celda empieza vacia
    }
    // Destructor                    // Se ejecuta solo cuando el objeto desaparece
    ~Mundo( )
    { delete [ ] celdas; }           // Devuelve la memoria al heap (con [ ] porque es un arreglo)

    // [NUEVO] Getters: los necesita colocarEspecies() para validar que la fila/columna
    //         que escribe el jugador este dentro del tablero.
    int getFilas( ) const    { return filas; }
    int getColumnas( ) const { return columnas; }

    // Returns what is in cell (i, j)
    int obtenerCelda( int i, int j ) const    // const: promete no modificar el objeto
    { return celdas[ i * columnas + j ]; }    // Convierte (fila, columna) en una posicion del arreglo

    void ponerCelda( int i, int j, int especie )
    { celdas[ i * columnas + j ] = especie; } // Guarda la especie en esa posicion

    // Prints the world on the screen
    // [MODIFICADO] parametro opcional conIndices: si es true, imprime numeros de fila y
    //              columna alrededor del tablero (lo usa la fase de colocacion para que el
    //              jugador sepa las coordenadas). Con conIndices = false el resultado es
    //              identico al original, asi la simulacion se ve igual que antes.
    void mostrar( bool conIndices = false ) const
    {
      if( conIndices )                        // Encabezado con el numero de cada columna
      {
        cout << "    ";                        // Hueco para la etiqueta de fila
        for( int j = 0; j < columnas; j++ )
          cout << setw( 3 ) << j;
        cout << endl;
      }
      for( int i = 0; i < filas; i++ )        // Recorre las filas, de arriba hacia abajo
      {
        if( conIndices )
          cout << setw( 4 ) << i;             // Etiqueta de la fila a la izquierda
        for( int j = 0; j < columnas; j++ )   // Recorre las columnas de esa fila
        {
          int valor = obtenerCelda( i, j );   // Lee que hay en la celda (i, j)
          char ch;
          if( valor == 0 )      ch = '.';     // Vacia: punto
          else if( valor == 1 ) ch = 'A';     // Especie 1
          else if( valor == 2 ) ch = 'B';     // Especie 2
          else                  ch = 'C';     // Especie 3
          if( conIndices )
            cout << setw( 3 ) << ch;          // Alineado con el encabezado
          else
            cout << ch << ' ';                // Formato original: letra + espacio
        }
        cout << endl;                         // Al terminar la fila, salta de linea
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
          int especie = obtenerCelda( i, j );     // Que individuo hay aqui
          if( especie == 0 )
            continue;                               // Celda vacia: nada que hacer
          if( rand( ) % 100 < probMuerte( especie ) )   // Muere de forma natural?
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
        return 7;                      // A: la mas mortal
      else if( especie == 2 )
        return 4;                      // B: normal
      else
        return 2;                      // C: la que mas vive
    }
};                                   // cierre de la clase

// ===========================================================================
// [NUEVO] Funciones de interaccion (fuera de la clase).
// ===========================================================================

// Lee un entero por teclado y no devuelve hasta que este dentro de [minv, maxv].
// Si el usuario escribe basura (una letra, por ejemplo) limpia el buffer y vuelve a pedir.
// Si se acaba la entrada (EOF / Ctrl+Z / Ctrl+D) corta el programa en vez de quedarse
// en un bucle infinito.
int leerEnteroEnRango( const string & prompt, int minv, int maxv )
{
  int v;
  while( true )
  {
    cout << prompt;
    if( cin >> v && v >= minv && v <= maxv )
      return v;                                   // Valor valido: listo
    if( cin.eof( ) )                              // Ya no hay mas entrada
    {
      cout << endl;
      exit( 0 );
    }
    cout << "Valor invalido. Debe estar entre " << minv << " y " << maxv << "." << endl;
    cin.clear( );                                 // Quita el estado de error de cin
    cin.ignore( numeric_limits<streamsize>::max( ), '\n' );   // Descarta el resto de la linea
  }
}

// Pantalla de inicio: muestra el titulo, la leyenda de especies y el menu.
// Devuelve 1 (nueva simulacion) o 2 (salir).
int pantallaInicio( )
{
  cout << endl;
  cout << "========================================" << endl;
  cout << "        SIMULADOR DE ECOSISTEMA"          << endl;
  cout << "========================================" << endl;
  cout << "  Especies:  A = 1    B = 2    C = 3"     << endl;
  cout << "----------------------------------------" << endl;
  cout << "  1. Nueva simulacion"                    << endl;
  cout << "  2. Salir"                               << endl;
  cout << "----------------------------------------" << endl;
  return leerEnteroEnRango( "  Opcion: ", 1, 2 );
}

// Fase de colocacion: el jugador pone las especies donde quiera antes de simular.
// Repite: muestra el tablero con coordenadas, pregunta especie y celda, y la coloca.
// Termina cuando el jugador escribe -1.
void colocarEspecies( Mundo & m )
{
  cout << endl << "Coloca las especies en el tablero." << endl;
  while( true )
  {
    cout << endl;
    m.mostrar( true );                            // Tablero con numeros de fila/columna
    int especie = leerEnteroEnRango(
      "\nEspecie (1=A, 2=B, 3=C, 0=borrar, -1=empezar): ", -1, 3 );
    if( especie == -1 )
      break;                                      // El jugador decidio arrancar la simulacion
    int i = leerEnteroEnRango( "  Fila: ",    0, m.getFilas( )    - 1 );
    int j = leerEnteroEnRango( "  Columna: ", 0, m.getColumnas( ) - 1 );
    m.ponerCelda( i, j, especie );                // 0 borra, 1/2/3 coloca especie
  }
}

// Main function
int main( )
{
  srand( (unsigned) time( 0 ) );     // Semilla: hace que el azar cambie en cada ejecucion

  while( true )                      // [NUEVO] Vuelve al menu al terminar cada simulacion
  {
    int op = pantallaInicio( );
    if( op == 2 )                    // Salir
    {
      cout << endl << "Hasta luego!" << endl;
      break;
    }

    // Tamano del tablero (antes estaba fijo en 5x5)
    int filas    = leerEnteroEnRango( "\nFilas del tablero (3-20): ",    3, 20 );
    int columnas = leerEnteroEnRango( "Columnas del tablero (3-20): ",   3, 20 );
    Mundo m( filas, columnas );

    colocarEspecies( m );            // El jugador arma el mundo inicial

    // Cuantos turnos correr (el port los tenia fijos en 12; lo dejo elegible).
    int turnos = leerEnteroEnRango( "\nCuantos turnos simular? (1-100): ", 1, 100 );
    for( int t = 0; t < turnos; t++ )
    {
      cout << endl << "Turno " << t << endl;
      m.mostrar( );                  // Simulacion: mismo formato que el original
      m.avanzarTurno( );
    }

    cout << endl << "Simulacion terminada. Enter para volver al menu...";
    cin.ignore( numeric_limits<streamsize>::max( ), '\n' );   // Descarta el \n pendiente
    cin.get( );                      // Espera el Enter real
  }

  return 0;
}