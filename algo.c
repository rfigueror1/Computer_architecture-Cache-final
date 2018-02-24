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
