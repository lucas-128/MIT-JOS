TP3: Multitarea con desalojo
============================

sys_yield
---------
Código modificado en la función i386() para correr 3 instancias del programa definido en yield.c

	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);
	ENV_CREATE(user_yield, ENV_TYPE_USER);

Se obtuvo la siguiente salida:

```
Booting from Hard Disk..6828 decimal is 15254 octal!
Physical memory: 131072K available, base = 640K, extended = 130432K
check_page_free_list() succeeded!
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
[00000000] new env 00001001
[00000000] new env 00001002
Hello, I am environment 00001000.
Hello, I am environment 00001001.
Hello, I am environment 00001002.
Back in environment 00001000, iteration 0.
Back in environment 00001001, iteration 0.
Back in environment 00001002, iteration 0.
Back in environment 00001000, iteration 1.
Back in environment 00001001, iteration 1.
Back in environment 00001002, iteration 1.
Back in environment 00001000, iteration 2.
Back in environment 00001001, iteration 2.
Back in environment 00001002, iteration 2.
Back in environment 00001000, iteration 3.
Back in environment 00001001, iteration 3.
Back in environment 00001002, iteration 3.
Back in environment 00001000, iteration 4.
All done in environment 00001000.
[00001000] exiting gracefully
[00001000] free env 00001000
Back in environment 00001001, iteration 4.
All done in environment 00001001.
[00001001] exiting gracefully
[00001001] free env 00001001
Back in environment 00001002, iteration 4.
All done in environment 00001002.
[00001002] exiting gracefully
[00001002] free env 00001002
No runnable environments in the system!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
```
Dentro del programa yield.c se llama a la syscall sys_yield la cual cede el control del cpu del proceso actual. 
El scheduler actualmente está implementado sin desalojo de procesos, por lo que depende de que cada proceso ceda 
el control. En este caso, cuando el environment 0001000 cede el control, 
el scheduler ejecuta el 00001001 que a su vez cede el control para ejecutar el environment 00001002 
y así sucesivamente de forma circular se recorren los environments que se están ejecutando 
hasta que cada uno finaliza su ciclo.

dumbfork
--------
1- Si una página no es modificable en el padre ¿lo es en el hijo? En otras palabras: ¿se preserva, en el hijo, el flag de solo-lectura en las páginas copiadas?
<br /><br />
En dumbfork, el adress space del padre se copia usando:
```
	for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE)
		duppage(envid, addr);
```
Dentro de duppage, las páginas nuevas se allocan usando page alloc y se le pasa por parámetro los flags: 
```
(PTE_P | PTE_U | PTE_W)
```
Independiente de los flags que tenga la página en el proceso padre. 
Entonces en el hijo no se preserva el flag de solo-lectura en las páginas copiadas.
<br /><br />

2- Mostrar, con código en espacio de usuario, cómo podría dumbfork() verificar si 
una dirección en el padre es de solo lectura, de tal manera que pudiera pasar 
como tercer parámetro a duppage() un booleano llamado readonly que indicase 
si la página es modificable o no:

```
pte_t uvpt[];     // VA of "virtual page table"
pde_t uvpd[];     // VA of current page directory

envid_t dumbfork(void) {
    for (addr = UTEXT; addr < end; addr += PGSIZE) {
        bool readonly = true;
        
        // Verificar que la pagina este mapeada 
        if ((uvpd[PDX(addr)] & PTE_P) != 0 && (uvpt[PGNUM(addr)] & PTE_P) != 0) {
            if (uvpt[PGNUM(addr)] & PTE_W) { // ver si tiene el flag de escritura
                readonly = false;
            }
        }
        duppage(envid, addr, readonly);
    }
```
<br />
3- Supongamos que se desea actualizar el código de duppage() para tener en 
cuenta el argumento readonly: si este es verdadero, la página copiada no 
debe ser modificable en el hijo. Es fácil hacerlo realizando una última llamada 
a sys_page_map() para eliminar el flag PTE_W en el hijo, cuando corresponda:

```
void duppage(envid_t dstenv, void *addr, bool readonly) {
    // Código original (simplificado): tres llamadas al sistema.
    sys_page_alloc(dstenv, addr, PTE_P | PTE_U | PTE_W);
    sys_page_map(dstenv, addr, 0, UTEMP, PTE_P | PTE_U | PTE_W);

    memmove(UTEMP, addr, PGSIZE);
    sys_page_unmap(0, UTEMP);

    // Código nuevo: una llamada al sistema adicional para solo-lectura.
    if (readonly) {
        sys_page_map(dstenv, addr, dstenv, addr, PTE_P | PTE_U);
    }
}
```
Esta versión del código, no obstante, incrementa las llamadas al sistema que 
realiza duppage() de tres, a cuatro. Se pide mostrar una versión en el que se 
implemente la misma funcionalidad readonly, pero sin usar en ningún caso más de 
tres llamadas al sistema.


Se puede chequear si se trata de un caso de solo-lectura para cambiar los flags 
que se utilizan como parámetros y así reducir el número de llamadas al sistema.
```
void duppage(envid_t dstenv, void *addr, bool readonly) {
     
    perm = PTE_U | PTE_P;
    if (!readonly) {
        perm = perm | PTE_W;
    }
    
    sys_page_alloc(dstenv,addr,perm);
    sys_page_map(dstenv,addr,0,UTEMP,perm);
    
    memmove(UTEMP,addr,PGSIZE);
    
    sys_page_unmap(0,UTEMP);
}
```

ipc_recv
--------

Un proceso podría intentar enviar el valor númerico -E_INVAL vía ipc_send(). 
¿Cómo es posible distinguir si es un error, o no?

```
envid_t src = -1;
int r = ipc_recv(&src, 0, NULL);

if (r < 0)
  if (/* ??? */)
    puts("Hubo error.");
  else
    puts("Valor negativo correcto.")
```
La firma de ipc_recv:

```
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
```
Se llama con los valores:
- from_env_store = -1;
- pg = 0;
- perm_store = NULL;

Dentro de ipc_recv se usa la syscall ```sys_ipc_recv(pg);``` Sabemos por la descripción 
de la función que si falla la syscall y el valor ```from_env_store``` no es nulo, 
se le asigna un 0 y devuelve el error.
```
    //  If the system call fails, then store 0 in *fromenv and *perm (if
    //  they're nonnull) and return the error.
```
Entonces para distinguir si fue un error habria que controlar el valor de ```src```:
```
  if (src == 0)
    puts("Hubo error.");
```


sys_ipc_try_send
----------------

Una posible implementación de sys_ipc_send() bloqueante:

En el struct env, habría que cambiar los atributos:
- ```bool env_ipc_recving``` por ```bool env_ipc_sending``` para marcar que el proceso esta esperando para enviar.
- ```envid_t env_ipc_from``` por ```envid_t env_ipc_to``` para guardar el envid del proceso que envía (ipc_recv usa esta
  referencia para despertar a dicho proceso)

```sys_ipc_send()``` Deberá setear el proceso que intenta enviar como ```ENV_NOT_RUNNABLE``` para ponerlo a dormir 
esperando a que otro proceso intente recibir.

```sys_ipc_recv()``` Debera:
- Intentar recibir un mensaje.
- Si no hay ningún proceso enviando entonces usa la syscall ```sys_yield()```
para ceder el control del cpu.
- Una vez que logra recibir algo, despertar al proceso que lo llamo marcándolo como ```ENV_RUNNABLE```


Si varios procesos (A₁, A₂, …) intentan enviar a B, lo más razonable seria que despierten en el orden que se ejecutaron.
Se debería almacenar una lista de procesos en espera para ir desencolando de manera ordenada donde
cada proceso que llame a ```sys_ipc_send()``` se suma a esa lista.