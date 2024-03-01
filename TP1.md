TP1: Memoria virtual en JOS
===========================

boot_alloc_pos
--------------
A. 

Podemos asumir que el valor de end ese será el valor devuelto por `boot_alloc()`
en la primera ejecución: f0114950. Aunque claro, esto deberá ser alineado a PGSIZE (4096). Así que
tenemos que realizar una corrección:

Pasamos end a decimal
```
0xf0114950 = 4027664720 
```
Vemos si es múltiplo de 4096
```
4027664720/4096 = 983316.5820
```
Como no es múltiplo, buscamos el múltiplo más cercano (lo que haría la alineación).
```
983317×4096 = 4027666432
```
Y finalmente pasamos el resultado a hexadecimal.
```
4027666432 = F0115000
```

El valor de end sale de los siguientes comandos:

> $ readelf -a obj/kern/kernel

Output resumido:

```
...
96: f0101ba4   222 FUNC    GLOBAL DEFAULT    1 strtol
97: f0101927    31 FUNC    GLOBAL DEFAULT    1 strnlen
98: f0100e5a   181 FUNC    GLOBAL DEFAULT    1 mem_init
99: f0101966    34 FUNC    GLOBAL DEFAULT    1 strcat
100: f0114940     4 OBJECT  GLOBAL DEFAULT    6 panicstr
101: f0114950     0 NOTYPE  GLOBAL DEFAULT    6 end
102: f01000e4    58 FUNC    GLOBAL DEFAULT    1 _warn
103: f0101a6b    28 FUNC    GLOBAL DEFAULT    1 strfind
104: f0101ec9     0 NOTYPE  GLOBAL DEFAULT    1 etext
...
```

> $ nm obj/kern/kernel

Output resumido:

```
...
f01010ed T debuginfo_eip
f0100132 t delay
f0114300 D edata
f0114950 B end
f010000c T entry
f0112000 D entry_pgdir
f0113000 D entry_pgtable
f0102804 r error_string
f0101ec9 T etext
f0100733 T getchar
...
```

B.

Al ejecutar los comandos sugeridos por el enunciado conseguimos la variable end: `f0114950`

```
(gdb) finish
Run till exit from #0  boot_alloc (n=4096) at kern/pmap.c:98
Could not fetch register "orig_eax"; remote failure reply 'E14'

```
Como da este error, imprimimos lo que devuelve la función.

```
=> 0xf0100eec <mem_init+32>:    mov    0xf0118b88,%eax
153             memset(kern_pgdir, 0, PGSIZE);
(gdb) print kern_pgdir
$2 = (pde_t *) 0xf0119000

```


map_region_large
----------------


## ¿cuánta memoria se ahorró de este modo? (en KiB)

Por cada large page que utilizamos nos ahorramos una tabla de nivel 2.
Cada tabla de nivel 2 tiene 1024 entradas de 32 bits (4 bytes), es decir, ocupa 1024 * 4 = 4 Kib.

En Jos, llamamos a boot_map_region 3 veces durante el proceso de arranque. Para ver cuanta memoria nos ahorramos,
necesitamos saber si se cumplen las condiciones para que se usen large pages.

1) Mapeo de arreglo pages. Como el arreglo no ocupa más de 4MB, no se utilizan large pages.
2) Mapeo del kernel. Como KSTKSIZE no ocupa más de 4MB, no se utilizan large pages.
3) Mapeo de toda la memoria física. Como son 256MB lo que se tiene que mapear, se utilizan 64 large pages con
   lo que nos termina ahorrando 64*4kb = 256 Kib.

## ¿es una cantidad fija, o depende de la memoria física de la computadora?

No es una cantidad fija ya que si estuviéramos en otra arquitectura las tablas ocuparían cantidades diferentes de memoria.
