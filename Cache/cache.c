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

cache init_cache_a_cache(cache c, int size)
{
  Pcache cache_init;
  cache_init = &c;
  // Inicializando cache_init.
  cache_init -> size = size;
  cache_init -> associativity = cache_assoc;
  cache_init -> n_sets        = size / (cache_assoc * cache_block_size);
  bitSet = LOG2(cache_init -> n_sets);
  bitOffset = LOG2(cache_block_size);
  tagMask = MASK_ORIG << (bitOffset + bitSet);
  cache_init -> index_mask_offset = bitOffset;
  //mascara que ayuda a obtener el indice de un numero
  int mask_auxiliar = (1<<bitSet)-1;

  cache_init -> index_mask    = mask_auxiliar << bitOffset;


  cache_init -> LRU_head      = (Pcache_line*)malloc(sizeof(Pcache_line) * cache_init -> n_sets);
  cache_init -> LRU_tail      = (Pcache_line*)malloc(sizeof(Pcache_line) * cache_init -> n_sets);

  for(int i=0; i<cache_init->n_sets; i++){
    //Inicialimos las lineas cabeza de cada set
    cache_init -> LRU_head[i] = malloc(sizeof(cache_line));
    cache_init -> LRU_head[i] -> tag = -1;
    cache_init -> LRU_head[i] -> dirty = -1;
    Pcache_line temp = cache_init -> LRU_head[i];
    for(int j = cache_assoc; j > 1 ; j--){
      temp -> LRU_next = malloc(sizeof(cache_line));
      temp -> LRU_next -> tag = -1;
      temp -> LRU_next -> dirty = -1;
      temp = temp -> LRU_next;
    }
    cache_init -> LRU_tail[i] = temp;

  }
  return(c);

}

void init_cache(){
  if(!cache_split){
    c1 = init_cache_a_cache(c1, cache_usize);
  } else {
    c1 = init_cache_a_cache(c1, cache_isize);
    c2 = init_cache_a_cache(c2, cache_dsize);
  }
}

/************************************************************/

/************************************************************/
void perform_access_set_assoc(cache c, unsigned addr, unsigned access_type)
{
  // Dirección y tag
  int index;
  unsigned int tag;
  Pcache cache_aux;
  cache_aux = &c;
  index = (addr & c.index_mask) >> c.index_mask_offset;
  tag = addr & tagMask;
  // Inicializa la busqueda del tag.
  int found = 0;
  int count_assoc = 1;
  int help = 1;
  // Apuntador que ayuda a avanzar líneas dentro del set.
  Pcache_line temp = cache_aux -> LRU_head[index];
  // Buscar el tag dentro de el set.
  while(!found && (count_assoc <= (cache_aux -> associativity)) && help){
    if((temp -> tag) == tag){
        found = 1;
    } // Mientras no sea el último y que el siguiente no sea nulo.
    else if(count_assoc  < (cache_aux -> associativity) && temp -> LRU_next){
      temp = temp -> LRU_next; // Seguimos con la siguiente línea en el set.
      count_assoc++;           // Contador del número de líneas en el set.
    }
    // Condición para no llegar hasta el último en el set
    // si es que no tienen nada.
    else {
      help = 0;
    }
  }
  Pcache_line item = malloc(sizeof(cache_line));
  switch (access_type) {
/******************************************************************************/
/*--------------------------Load Data-----------------------------------------*/
/******************************************************************************/
    case TRACE_DATA_LOAD:
    cache_stat_data.accesses++;
      if(!found){
        cache_stat_data.misses++;
        // Si el bloque no está vacío.
        if((count_assoc) == (cache_aux -> associativity) && temp -> tag != -1){
          cache_stat_data.replacements++;
          if(temp -> dirty){
            // Escribimos en memoria principal su contenido.
            cache_stat_data.copies_back += words_per_block;
          }
        }
        // Leer de memoria principal el dato.
        cache_stat_data.demand_fetches += words_per_block;
        item -> tag = tag;
        item -> dirty = 0;
        insert(
          &(cache_aux -> LRU_head[index]),
          &(cache_aux -> LRU_tail[index]),
          item);
      } else {
          delete(
            &(cache_aux -> LRU_head[index]), // Head
            &(cache_aux -> LRU_tail[index]), // Tail
            temp
          );                     // Bloque a insertar
          insert(
            &(cache_aux -> LRU_head[index]), // Head
            &(cache_aux -> LRU_tail[index]), // Tail
            temp
          );                     // Bloque a insertar
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
       if(!cache_writealloc){
         cache_stat_data.copies_back++;
         item -> dirty = 0;
       }
       else{
         if((count_assoc) == (cache_aux -> associativity) && temp -> tag != -1){
           cache_stat_data.replacements++;
           if(temp -> dirty && cache_writeback){
             // Escribimos en memoria principal su contenido.
             cache_stat_data.copies_back += words_per_block;
           }

         }
         // Leer de memoria principal el dato.
         cache_stat_data.demand_fetches += words_per_block;
         item -> tag = tag;
         item -> dirty = 1;
         if(!cache_writeback){
           item -> dirty = 0;
           cache_stat_data.copies_back += 1;
         }
         insert(
          &cache_aux -> LRU_head[index],
          &cache_aux -> LRU_tail[index],
          item
         );
       }
       // Si la dirección es distinta y no vacía.
     } // Sí encontró el bloque.
     else {
        delete(
          &(cache_aux -> LRU_head[index]), // Head
          &(cache_aux -> LRU_tail[index]), // Tail
          temp                             // Bloque a borrar
        );
        temp -> dirty = 1;

        if(!cache_writeback){
          temp -> dirty = 0;
          cache_stat_data.copies_back += 1;
        }

        insert(
          &(cache_aux -> LRU_head[index]), // Head
          &(cache_aux -> LRU_tail[index]), // Tail
          temp                             // Bloque a insertar
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
        if((count_assoc) == (cache_aux -> associativity) && temp -> tag != -1){
          // Aumenta reemplazos.
          cache_stat_inst.replacements++;
          // Conserva el dirty bit.
          // Si dirty bit es 1
          if(temp -> dirty){
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
          &(cache_aux -> LRU_head[index]),
          &(cache_aux -> LRU_tail[index]),
          item
        );
      } // Sí lo encontró.
      else {
          delete(
            &(cache_aux -> LRU_head[index]), // Head
            &(cache_aux -> LRU_tail[index]), // Tail
            temp                             // Bloque a borrar
          );
          insert(
            &(cache_aux -> LRU_head[index]), // Head
            &(cache_aux -> LRU_tail[index]), // Tail
            temp                             // Bloque a insertar
          );
      }
     break;
  }

}

void perform_access(addr, access_type)
  unsigned addr, access_type;
  {
    // Acceso unificado
    if(!cache_split){
      perform_access_set_assoc(c1, addr, access_type);

    } else {
      switch (access_type){
        case TRACE_DATA_LOAD: case TRACE_DATA_STORE:
        perform_access_set_assoc(c2, addr, access_type);
        break;
        case TRACE_INST_LOAD:
        perform_access_set_assoc(c1, addr, access_type);
        break;
      }

    }

  }

int cp_flush(cache c){
  int copies_back = 0;
   Pcache cache_aux;
   cache_aux = &c;
  for(int i=0; i < cache_aux->n_sets; i++){
    Pcache_line temp = cache_aux -> LRU_head[i];
    int count_assoc = 1;
    while(count_assoc <= (cache_aux -> associativity)){
      if(temp -> dirty == 1){
          copies_back += words_per_block;
      }
      if(temp -> LRU_next){
        temp = temp -> LRU_next;
      }
      count_assoc++;
    }
  }
  return(copies_back);
}

void flush()
{
  int help;
  if(!cache_split){
    cache_stat_data.copies_back += cp_flush(c1);
  }
  else {
    cache_stat_data.copies_back += cp_flush(c2);
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
    printf("  Split I-D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  } else {
    printf("  Unified I-D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t\t%d\n", cache_block_size);
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
