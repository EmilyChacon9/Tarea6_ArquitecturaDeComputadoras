# Tarea6_ArquitecturaDeComputadoras

El simulador fue implementado inicialmente en C++ para poder ejecutarse en la terminal, pero se decidió utilizar HTML en Visual Studio Code para poder utilizar una versión en el navegador que funcione lo más cercano a CPUlator. En la presente tarea se entregan ambas versiones del código.

En ambas versiones se implementaron instrucciones ISA en base RV32I, incluyendo las 37 instrucciones que se pedían: loads, stores, instrucciones aritméticas o con inmediatos, operaciones lógicas, instrucciones de desplazamiento, los saltos y las ramas condicionales. Se consideró un espacio de direcciones de 32 bits.

El simulador es capaz de cargar programas desde archivos binarios crudos (o raw) .bin al address 0x00000000 y soportar la ejecución paso a paso de cada instrucción.
En la versión del terminal, el usuario puede inspeccionar el PC, los valores de los registros (todos o los que especifique) y los rangos de memoria mediante comandos definidos, los cuales se pueden explorar a través de un comando ‘help’. 

La versión web añade una interfaz visual con un panel de todos los registros que se actualiza cuando se ejecutan las instrucciones y regresan a sus valores iniciales al restaurarse el programa. También contiene una consola que acepta los mismos comandos que la versión de terminal pura.

