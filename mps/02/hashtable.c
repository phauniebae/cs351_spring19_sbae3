#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

/* Daniel J. Bernstein's "times 33" string hash function, from comp.lang.C;
   See https://groups.google.com/forum/#!topic/comp.lang.c/lSKWXiuNOAk */
unsigned long hash(char *str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

hashtable_t *make_hashtable(unsigned long size) {
  hashtable_t *ht = malloc(sizeof(hashtable_t));
  ht->size = size;
  ht->buckets = calloc(sizeof(bucket_t *), size);
  return ht;
}

void ht_put(hashtable_t *ht, char *key, void *val) {
  unsigned int idx = hash(key) % ht->size;
  bucket_t *b = ht->buckets[idx];
  while (b) {
    if (strcmp(b->key, key) == 0) {
	 free(b->val);
	 free(key);
	 b->val = val;
	 return;
    }  
    b = b->next;
  }    
  b = malloc(sizeof(bucket_t));
  b->key = key;
  b->val = val;
  b->next = ht->buckets[idx];
  ht->buckets[idx] = b;
}

void *ht_get(hashtable_t *ht, char *key) {
  unsigned int idx = hash(key) % ht->size;
  bucket_t *b = ht->buckets[idx];
  while (b) {
    if (strcmp(b->key, key) == 0) {
      return b->val;
    }
    b = b->next;
  }
  return NULL;
}

void ht_iter(hashtable_t *ht, int (*f)(char *, void *)) {
  bucket_t *b;
  unsigned long i;
  for (i=0; i<ht->size; i++) {
    b = ht->buckets[i];
    while (b) {
      if (!f(b->key, b->val)) {
        return ; // abort iteration
      }
      b = b->next;
    }
  }
}

void free_hashtable(hashtable_t *ht) {
        for(int i=0; i<ht->size; i++) {
                bucket_t *b = ht->buckets[i];
                while(b){
                        free(b->key);
			free(b->val);
			bucket_t *c = b;
			b = b->next;
			free(c);
                }
        }
	free(ht->buckets);
	free(ht); // FIXME: must free all substructures!
}

/* TODO */
void  ht_del(hashtable_t *ht, char *key) {
	unsigned int idx = hash(key) % ht->size;
	if (strcmp(ht->buckets[idx]->key,key) == 0){
		ht->buckets[idx] = ht->buckets[idx]->next;	
		return;
	}	
	bucket_t *b = ht->buckets[idx];
	while(b && b->next){
		if (strcmp(b->next->key, key) == 0){
			b->next = b->next->next;
			return;
		}
		b = b->next;
	}
}

void  ht_rehash(hashtable_t *ht, unsigned long newsize) {
	hashtable_t *newht = make_hashtable(newsize);
	for(int i=0; i<ht->size; i++) {
		bucket_t *b = ht->buckets[i];
		while(b){
			ht_put(newht, b->key, b->val);
			b = b->next;
		}
	}
	*ht = *newht;
}
