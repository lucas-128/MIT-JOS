TP4: Sistema de archivos e intérprete de comandos
=================================================

caché de bloques
----------------

Se recomienda leer la función diskaddr() en el archivo fs/bc.c.<br/>
```void *
diskaddr(uint32_t blockno)
{
if (blockno == 0 || (super && blockno >= super->s_nblocks))
panic("bad block number %08x in diskaddr", blockno);
return (char *) (DISKMAP + blockno * BLKSIZE);
}
```

- ¿Qué es super->s_nblocks? <br/>
```super->s_nblocks```
es la variable que guarda el numero total de bloques del disco.
<br/>
En la funcion
```diskaddr()```
vemos como se compara con
```blockno```
para ver si es menor y en tal caso producir un panic.


- ¿Dónde y cómo se configura este bloque especial? <br/>
  En la funcion
  ```opendisk()```
  del archivo
  ```fsformat.c```
  se configuran los atributos del ```struct super```
  ```
  alloc(BLKSIZE);
  super = alloc(BLKSIZE);
  super->s_magic = FS_MAGIC;
  super->s_nblocks = nblocks;
  super->s_root.f_type = FTYPE_DIR;
  strcpy(super->s_root.f_name, "/");
  ```
