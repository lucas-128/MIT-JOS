// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW 0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// En primer lugar, se verifica que la falla
	// recibida se debe a una escritura en
	// una región copy-on-write. Así, el manejador
	// llamará a panic() en cualquiera de los siguientes casos:
	//
	//     - el error ocurrió en una dirección no mapeada
	//     - el error ocurrió por una lectura y no una escritura
	//     - la página afectada no está marcada como copy-on-write

	// Ayuda para la primera parte: El error ocurrió por una lectura
	//     si el bit FEC_WR está a 0 en utf->utf_err;
	//     la dirección está mapeada si y solo sí el bit FEC_PR está a 1.
	//     Para verificar PTE_COW se debe usar uvpt.

	// LAB 4: Your code here.
	if ((err & FEC_WR) == 0 || (uvpd[PDX(addr)] & PTE_P) == 0 ||
	    (uvpt[PGNUM(addr)] & PTE_COW) == 0)
		panic("Pagefault incorrecto!\n");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	// se reserva una nueva página en una dirección temporal (PFTEMP)
	if ((r = sys_page_alloc(0, (void *) PFTEMP, PTE_U | PTE_W | PTE_P)) < 0)
		panic("sys page alloc error: %e\n", r);

	// escribir en ella los contenidos apropiados
	memcpy((void *) PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);

	// tras escribir los contenidos apropiados, se mapea en la dirección destino
	if ((r = sys_page_map(0,
	                      (void *) PFTEMP,
	                      0,
	                      ROUNDDOWN(addr, PGSIZE),
	                      PTE_U | PTE_W | PTE_P)) < 0)
		panic("sys page alloc error: %e\n", r);

	// se elimina el mapeo usado en la dirección temporal
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys page unmap error: %e\n", r);
}
//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	// LAB 4: Your code here.

	// La función duppage() es conceptualmente similar a dup_or_share() pero jamás reserva
	// memoria nueva (no llama a sys_page_alloc), ni tampoco la escribe.

	// Dado un número de página virtual n, se debe
	// mapear en el hijo la página física correspondiente
	// en la misma página virtual (usar sys_page_map).

	void *va = (void *) (pn * PGSIZE);

	// si una página tiene el bit PTE_SHARE,
	// se comparta con el hijo con los mismos permisos.
	if (uvpt[pn] & PTE_SHARE) {
		if ((r = sys_page_map(
		             0, va, envid, va, (int) uvpt[pn] & PTE_SYSCALL)) < 0)
			panic("sys_page_map: %e\n", r);

		// Si los permisos incluyen COW o Write → mapear con permiso COW
	} else if (uvpt[pn] & (PTE_W | PTE_COW)) {
		// Los permisos en el hijo son los de la página
		// original menos PTE_W; si y solo si se sacó PTE_W:
		// se añade el bit especial PTE_COW
		if ((r = sys_page_map(0, va, envid, va, PTE_P | PTE_U | PTE_COW)) <
		    0)
			panic("sys_page_map: %e\n", r);

		// Si los permisos resultantes en el hijo incluyen
		// PTE_COW, se debe remapear la página
		// en el padre con estos permisos.
		if ((r = sys_page_map(0, va, 0, va, PTE_P | PTE_U | PTE_COW)) < 0)
			panic("sys_page_map: %e\n", r);

	} else {
		// Para las páginas de escritura no creará una copia,
		// sino que la mapeará como sólo lectura.
		if ((r = sys_page_map(0, va, envid, va, PTE_P | PTE_U)) < 0)
			panic("sys_page_map: %e\n", r);
	}

	return 0;  // Success
}

static void
dup_or_share(envid_t dstenv, void *va, int perm)
{
	/* La principal diferencia con la funcion duppage() de user/dumbfork.c es:
	 * si la página es de solo-lectura, se comparte en lugar de crear una copia. */

	int r;
	if (perm & PTE_W) {
		// Tiene permiso de escritura --> duppage de dumbfork
		if ((r = sys_page_alloc(dstenv, va, perm)) < 0)
			panic("sys_page_alloc: %e", r);

		if ((r = sys_page_map(dstenv, va, 0, UTEMP, perm)) < 0)
			panic("sys_page_map: %e", r);

		memmove(UTEMP, va, PGSIZE);

		if ((r = sys_page_unmap(0, UTEMP)) < 0)
			panic("sys_page_unmap: %e", r);

	} else {
		// No tiene permiso escritura --> se comparte en lugar de crear una copia.
		if ((r = sys_page_map(0, va, dstenv, va, perm)) < 0)
			panic("[dup_or_share] sys_page_map: %e", r);
	}
}

envid_t
fork_v0(void)
{
	// dumbfork.c
	envid_t envid;
	uint8_t *addr;
	int r;

	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We're the parent.
	// Eagerly copy our entire address space into the child.
	// This is NOT what you should do in your fork implementation.

	/* La copia del espacio de direcciones del padre al hijo difiere de
	dumbfork() de la siguiente manera: Se abandona el uso de end; en su
	lugar, se procesan página a página todas las direcciones desde 0 hasta
	UTOP. Si la página (dirección) está mapeada, se invoca a la función
	dup_or_share() */
	for (addr = (uint8_t *) 0; addr < (uint8_t *) UTOP; addr += PGSIZE) {
		// Verificar que la pagina este mapeada
		if ((uvpd[PDX(addr)] & PTE_P) != 0 &&
		    (uvpt[PGNUM(addr)] & PTE_P) != 0)
			dup_or_share(envid,
			             (void *) addr,
			             uvpt[PGNUM(addr)] & PTE_SYSCALL);
	}

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.

	//  El cuerpo de la función fork(2) es muy parecido al de
	//  dumbfork() y fork_v0(), vistas anteriormente.
	//  Se pide implementar la función fork(2) teniendo
	//  en cuenta la siguiente secuencia de tareas:

	//  1) Instalar, en el padre, la función pgfault como manejador de page
	//  faults. Ésto también reservará memoria para su pila de excepciones.
	set_pgfault_handler(pgfault);

	//  2) Llamar a sys_exofork() y manejar el resultado.
	//  En el hijo, actualizar, como de costumbre, la variable global thisenv.
	envid_t envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// 3) Reservar memoria para la pila de excepciones del hijo,
	//  e instalar su manejador de excepciones.
	sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	extern void _pgfault_upcall(void);
	sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

	//  4) Iterar sobre el espacio de memoria del padre (desde 0 hasta UTOP)
	//  como en forkv0.
	uint8_t *addr;
	for (addr = (uint8_t *) 0; addr < (uint8_t *) UTOP; addr += PGSIZE) {
		//  - se pide recorrer el número mínimo de páginas, esto es,
		//  no verificar ningún PTE cuyo PDE ya indicó que no hay page
		//  table presente
		if (!uvpd[PDX(addr)] & PTE_P)
			continue;

		// no se debe mapear la región correspondiente a la pila de excepciones;
		if ((uvpt[PGNUM(addr)] & PTE_P) &&
		    (addr < (uint8_t *) (UXSTACKTOP - PGSIZE)))
			//  Para cada página presente, invocar a la función
			//  duppage() para mapearla en el hijo.
			duppage(envid, PGNUM(addr));
	}

	//  5) Finalizar la ejecución de fork(2) marcando al proceso hijo como ENV_RUNNABLE.
	sys_env_set_status(envid, ENV_RUNNABLE);
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
