/*
 * cache.c
 */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE;
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;
static int bitSet;
static int bitOffset;
static int tagMask;
#define MASK_ORIG 0xFFFFFFFF
/* cache model data structures */
static Pcache icache;
static Pcache dcache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;
/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{

  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }

}
/************************************************************/

/************************************************************/
void init_cache()
{
  icache = &c1;
  // Inicializando icache.
  icache -> size = cache_usize;
  icache -> associativity = cache_assoc;
  icache -> n_sets        = cache_usize / (cache_assoc * cache_block_size);
  bitSet = LOG2(icache -> n_sets);
  bitOffset = LOG2(cache_block_size);
  tagMask = MASK_ORIG << (bitOffset + bitSet);
  icache -> index_mask_offset = bitOffset;
  //mascara que ayuda a obtener el indice de un numero
  int mask_auxiliar = (1<<bitSet)-1;

  icache -> index_mask    = mask_auxiliar << bitOffset;


  icache -> LRU_head      = (Pcache_line*)malloc(sizeof(Pcache_line) * icache -> n_sets);
  icache -> LRU_tail      = (Pcache_line*)malloc(sizeof(Pcache_line) * icache -> n_sets);
  // icache -> set_contents  = (int*)malloc(sizeof(int) * icache -> n_sets);
  // icache -> contents      = 0;
  for(int i=0; i<icache->n_sets; i++){
    //Inicialimos las lineas cabeza de cada set
    icache -> LRU_head[i] = malloc(sizeof(cache_line));
    icache -> LRU_head[i] -> tag = -1;
    icache -> LRU_head[i] -> dirty = -1;

    icache -> LRU_tail[i] = malloc(sizeof(cache_line));
    icache -> LRU_tail[i] -> tag = -1;
    icache -> LRU_tail[i] -> dirty = -1;

  }

  printf("Init cache: done.\n");
}

/************************************************************/

/************************************************************/
void perform_access_set_assoc(addr, access_type)
  unsigned addr, access_type;
{
  // Dirección y tag
  int index;
  unsigned int tag;
  index = (addr & icache->index_mask) >> icache->index_mask_offset;
  tag = addr & tagMask;


  // Buscar el tag dentro de el set.
  // Inicializa la busqueda del tag.
  int found = 0;
  int count_assoc = 1;
  Pcache_line temp = icache -> LRU_head[index];
  // printf("Inicia busqueda \n");
  while(found == 0 && count_assoc <= (icache -> associativity)){
    if((temp -> tag) == tag){
        found = 1;
    } // Mientras no sea el último y que el siguiente no sea nulo.
    else if(count_assoc <= (icache -> associativity) && (temp -> LRU_next)){
      temp = temp -> LRU_next; // Seguimos con el siguiente en el set
    }
    count_assoc++; // Contador del número de líneas en el set.
  }
  printf("%d-%d,%d,%d,%d \n", index, tag, access_type, found, tag);
  // printf("Termina busqueda \n");

  Pcache_line item = malloc(sizeof(cache_line));
  int dirty;

  switch (access_type) {
    case TRACE_DATA_LOAD:
    cache_stat_data.accesses++;
      if(!found){
        cache_stat_data.misses++;
        // Si la linea no está vacía.
        if(temp -> tag != -1){
          cache_stat_data.replacements++;
          dirty = temp -> dirty;
          delete(
            &(icache -> LRU_head[index]), // Head
            &(icache -> LRU_tail[index]), // Tail
            icache -> LRU_tail[index]
          );
          if(dirty == 1){
            // Escribimos en memoria principal su contenido.
            cache_stat_data.copies_back += words_per_block;
          }
        }

        // Leer de memoria principal el dato.
        cache_stat_data.demand_fetches += words_per_block;
        item -> tag = tag;
        item -> dirty = 0;
        insert(
          &(icache -> LRU_head[index]),
          &(icache -> LRU_tail[index]),
          item);
      } else {
          delete(
            &(icache -> LRU_head[index]), // Head
            &(icache -> LRU_tail[index]), // Tail
            temp);                     // Bloque a insertar
          insert(
            &(icache -> LRU_head[index]), // Head
            &(icache -> LRU_tail[index]), // Tail
            temp);                     // Bloque a insertar
      }
     break;
/******************************************************************************/
/*-------------------------Store Data-----------------------------------------*/
/******************************************************************************/
     case TRACE_DATA_STORE:
     cache_stat_data.accesses++;
     // Si es un miss.
     if(!found){
       // Aumenta estadística de misses
       cache_stat_data.misses++;
       // Si la dirección es distinta y no vacía.
       if(temp -> tag != -1){
         cache_stat_data.replacements++;
         dirty = icache -> LRU_tail[index] -> dirty;
         delete(
           &(icache -> LRU_head[index]),
           &(icache -> LRU_tail[index]),
           icache -> LRU_tail[index]
         );
         if(dirty == 1){
           // Escribimos en memoria principal su contenido.
           cache_stat_data.copies_back += words_per_block;
         }
       }

       // Leer de memoria principal el dato.
       cache_stat_data.demand_fetches += words_per_block;
       item -> tag = tag;
       item -> dirty = 1;
       insert(
        &icache -> LRU_head[index],
        &icache -> LRU_tail[index],
        item
       );
     } // Sí encontró el bloque.
     else {
        delete(
          &(icache -> LRU_head[index]), // Head
          &(icache -> LRU_tail[index]), // Tail
          temp                          // Bloque a borrar
        );
        temp -> dirty = 1;
        insert(
          &(icache -> LRU_head[index]), // Head
          &(icache -> LRU_tail[index]), // Tail
          temp                          // Bloque a insertar
        );
      }
      break;
    /******************************************************************************/
    /*----------------- Load Instructions-----------------------------------------*/
    /******************************************************************************/
     case TRACE_INST_LOAD:
     cache_stat_inst.accesses++;
     // Si es un miss
      if(!found){
        // Aumenta la estadística de misses.
        cache_stat_inst.misses++;
        // Si no está vacía la línea,
        if(temp -> tag != -1){
          // Aumenta reemplazos.
          cache_stat_inst.replacements++;
          // Conserva el dirty bit.
          dirty = temp -> dirty;
          // Borra la cola del set.
          delete(
            &(icache -> LRU_head[index]),
            &(icache -> LRU_tail[index]),
            icache -> LRU_tail[index]
          );
          // Si dirty bit es 1
          if(dirty == 1){
            // Escribimos en memoria principal su contenido.
            cache_stat_inst.copies_back += words_per_block;
          }
      }
        // Leer de memoria principal el dato.
        cache_stat_inst.demand_fetches += words_per_block;
        // Conservamos el tag y la instrucción no fue modificada.
        item -> tag = tag;
        item -> dirty = 0;
        // Insertamos la instrucción en el cache.
        insert(
          &(icache -> LRU_head[index]),
          &(icache -> LRU_tail[index]),
          item
        );
      } // Si lo encontró.
      else {
          delete(
            &(icache -> LRU_head[index]), // Head
            &(icache -> LRU_tail[index]), // Tail
            temp                          // Bloque a insertar
          );
          insert(
            &(icache -> LRU_head[index]), // Head
            &(icache -> LRU_tail[index]), // Tail
            temp                          // Bloque a insertar
          );
      }
     break;
  }

}

/************************************************************/

/************************************************************/
void perform_access_direct_mapping(addr, access_type)
  unsigned addr, access_type;
{
  int index;
  index = (addr & icache->index_mask) >> icache->index_mask_offset;
  unsigned int tag;
  tag = addr & tagMask;

  switch (access_type) {
/******************************************************************************/
/*--------------------------Load Data-----------------------------------------*/
/******************************************************************************/
    case TRACE_DATA_LOAD:
    cache_stat_data.accesses++;
    // Si es un miss
     if(icache->LRU_head[index]->tag != tag){
       cache_stat_data.misses++;
       // Si la linea no está vacía.
       if(icache -> LRU_head[index] -> tag != -1){
       cache_stat_data.replacements++;
     }
       // Si dirty bit es 1
       if(icache -> LRU_head[index] -> dirty == 1){
         // Escribimos en memoria principal su contenido.
         cache_stat_data.copies_back += words_per_block;
       }
       // Leer de memoria principal el dato.
       cache_stat_data.demand_fetches += words_per_block;
       icache -> LRU_head[index] -> tag = tag;
       // Hacer la línea como no dirty.
       icache -> LRU_head[index] -> dirty = 0;
     }
     break;
/******************************************************************************/
/*-------------------------Store Data-----------------------------------------*/
/******************************************************************************/
     case TRACE_DATA_STORE:
      cache_stat_data.accesses++;
      // Si es un miss.
      if(icache->LRU_head[index]->tag != tag){
        // Aumenta estadística de misses
        cache_stat_data.misses++;
        // Si la dirección es distinta y no vacía.
        if(icache -> LRU_head[index] -> tag != -1){
          cache_stat_data.replacements++;
        }
        // Si el dirty bit es 1.
        if(icache -> LRU_head[index] -> dirty == 1){
          // Escribimos en memoria principal su contenido.
          cache_stat_data.copies_back += words_per_block;
        }
        // Leer de memoria principal el dato.
        cache_stat_data.demand_fetches += words_per_block;
      }
      icache -> LRU_head[index] -> tag = tag;
      // Hacer la línea como dirty.
      icache -> LRU_head[index] -> dirty = 1;
      break;
/******************************************************************************/
/*----------------- Load Instructions-----------------------------------------*/
/******************************************************************************/
     case TRACE_INST_LOAD:
     cache_stat_inst.accesses++;
     // Si es un miss
      if(icache->LRU_head[index]->tag != tag){
        cache_stat_inst.misses++;
        if(icache -> LRU_head[index] -> tag != -1){
        cache_stat_inst.replacements++;
      }
        // Si dirty bit es 1
        if(icache -> LRU_head[index] -> dirty == 1){
          // Escribimos en memoria principal su contenido.
          cache_stat_inst.copies_back += words_per_block;
        }
        // Leer de memoria principal el dato.
        cache_stat_inst.demand_fetches += words_per_block;
        icache -> LRU_head[index]-> dirty = 0;
      }
      icache -> LRU_head[index] -> tag = tag;
      break;
    }
}

void perform_access(addr, access_type)
  unsigned addr, access_type;
  {
    //printf("Init perform access\n");
    if(cache_assoc > 1){
      //printf("Init perform access.\n");
      perform_access_set_assoc(addr, access_type);
      //printf("End perform access\n");
    } else{
      //printf("Something is wrong\n");
      perform_access_set_assoc(addr, access_type);
    }
  }

void flush()
{

  /* flush the cache */
  for(int i=0; i<icache->n_sets; i++){

    Pcache_line temp = icache -> LRU_head[i];
    int count_assoc = 1;
    while(count_assoc <= (icache -> associativity)){
      if((temp -> dirty) == 1){
          cache_stat_inst.copies_back += words_per_block;
      } else if(count_assoc != (icache -> associativity) && (temp -> LRU_next)){
        temp = temp -> LRU_next;
      }
      count_assoc++;
    }
  }

}
/************************************************************/

/************************************************************/
void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n",
	 cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
	 cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
	 (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
	 1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
	 (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
	 1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches +
	 cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
	 cache_stat_data.copies_back);
}
/************************************************************/
