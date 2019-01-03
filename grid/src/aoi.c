#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include "aoi.h"


#define INVALID_ID (~0)
#define PRE_ALLOC 16
//#define PRE_ALLOC 32
#define MODE_WATCHER 1
#define MODE_MARKER 2

typedef struct aoi_object {
	uint32_t id;
	int mode;
	float pos[3];
} aoi_object;

typedef struct aoi_map_slot {
	uint32_t id;
	aoi_object * obj;
	int next;
} aoi_map_slot;

typedef struct aoi_map {
	int size;
	int lastfree;
	aoi_map_slot * slot;
} aoi_map;

typedef struct aoi_set {
	int cap;
	int number;
	void **slot;
} aoi_set;

typedef struct aoi_tower {
	aoi_set *objects;
	int x,y,z;
} aoi_tower;

typedef struct aoi_space {
	float map_size[3];
	float tower_size[3];
	int tower_x_limit;
	int tower_y_limit;
	int tower_z_limit;
	aoi_tower *towers;
	aoi_map *objects;
	aoi_Alloc alloc;
	void *alloc_ud;
	enterAOI_Callback cb_enterAOI;
	leaveAOI_Callback cb_leaveAOI;
	void *cb_ud;
	aoi_set *set1;
	aoi_set *set2;
	aoi_set *result_set;
} aoi_space;


static aoi_object *
new_object(aoi_space * aoi, uint32_t id) {
	aoi_object * obj = aoi->alloc(aoi->alloc_ud, NULL, sizeof(*obj));
	obj->id = id;
	obj->mode = 0;
	return obj;
}

static void
delete_object(void *ud,aoi_object *obj) {
	aoi_space *aoi = ud;
	aoi->alloc(aoi->alloc_ud,obj,sizeof(*obj));
}

static aoi_object * map_get(aoi_map *m,uint32_t id);

static aoi_object *
get_object(aoi_space *aoi,uint32_t id) {
	return map_get(aoi->objects,id);
}

static inline aoi_map_slot *
mainposition(aoi_map *m , uint32_t id) {
	uint32_t hash = id & (m->size-1);
	return &m->slot[hash];
}

static void rehash(aoi_space * aoi, aoi_map *m);

static void
map_insert(aoi_space * aoi , aoi_map * m, uint32_t id , aoi_object *obj) {
	aoi_map_slot *s = mainposition(m,id);
	if (s->id == INVALID_ID) {
		s->id = id;
		s->obj = obj;
		return;
	}
	if (mainposition(m, s->id) != s) {
		aoi_map_slot * last = mainposition(m,s->id);
		while (last->next != s - m->slot) {
			assert(last->next >= 0);
			last = &m->slot[last->next];
		}
		uint32_t temp_id = s->id;
		aoi_object * temp_obj = s->obj;
		last->next = s->next;
		s->id = id;
		s->obj = obj;
		s->next = -1;
		if (temp_obj) {
			map_insert(aoi, m, temp_id, temp_obj);
		}
		return;
	} else if(s->obj == NULL) {
		s->id = id;
		s->obj = obj;
		return;
	}

	while (m->lastfree >= 0) {
		aoi_map_slot * temp = &m->slot[m->lastfree--];
		if (temp->id == INVALID_ID) {
			temp->id = id;
			temp->obj = obj;
			temp->next = s->next;
			s->next = (int)(temp - m->slot);
			return;
		}
	}
	rehash(aoi,m);
	map_insert(aoi, m, id , obj);
}

static void
rehash(aoi_space * aoi, aoi_map *m) {
	aoi_map_slot * old_slot = m->slot;
	int old_size = m->size;
	m->size = 2 * old_size;
	m->lastfree = m->size - 1;
	m->slot = aoi->alloc(aoi->alloc_ud, NULL, m->size * sizeof(aoi_map_slot));
	int i;
	for (i=0;i<m->size;i++) {
		aoi_map_slot * s = &m->slot[i];
		s->id = INVALID_ID;
		s->obj = NULL;
		s->next = -1;
	}
	for (i=0;i<old_size;i++) {
		aoi_map_slot * s = &old_slot[i];
		if (s->obj) {
			map_insert(aoi, m, s->id, s->obj);
		}
	}
	aoi->alloc(aoi->alloc_ud, old_slot, old_size * sizeof(aoi_map_slot));
}

static aoi_object *
map_get(aoi_map *m,uint32_t id) {
	aoi_map_slot *s = mainposition(m, id);
	for (;;) {
		if (s->id == id) {
			if (s->obj != NULL) {
				return s->obj;
			}
		}
		if (s->next < 0) {
			break;
		}
		s=&m->slot[s->next];
	}
	return NULL;
}

static void
map_foreach(aoi_map * m , void (*func)(void *ud, aoi_object *obj), void *ud) {
	int i;
	for (i=0;i<m->size;i++) {
		if (m->slot[i].obj) {
			func(ud, m->slot[i].obj);
		}
	}
}

static aoi_object *
map_remove(aoi_map *m, uint32_t id) {
	aoi_map_slot *s = mainposition(m,id);
	for (;;) {
		if (s->id == id) {
			aoi_object * obj = s->obj;
			s->obj = NULL;
			return obj;
		}
		if (s->next < 0) {
			return NULL;
		}
		s=&m->slot[s->next];
	}
}

static void
map_delete(aoi_space *aoi, aoi_map * m) {
	aoi->alloc(aoi->alloc_ud, m->slot, m->size * sizeof(aoi_map_slot));
	aoi->alloc(aoi->alloc_ud, m , sizeof(*m));
}

static aoi_map *
map_new(aoi_space *aoi) {
	int i;
	aoi_map * m = aoi->alloc(aoi->alloc_ud, NULL, sizeof(*m));
	m->size = PRE_ALLOC;
	m->lastfree = PRE_ALLOC - 1;
	m->slot = aoi->alloc(aoi->alloc_ud, NULL, m->size * sizeof(aoi_map_slot));
	for (i=0;i<m->size;i++) {
		aoi_map_slot * s = &m->slot[i];
		s->id = INVALID_ID;
		s->obj = NULL;
		s->next = -1;
	}
	return m;
}

static aoi_set *
set_new(aoi_space *aoi) {
	aoi_set *set = aoi->alloc(aoi->alloc_ud,NULL,sizeof(*set));
	set->cap = PRE_ALLOC;
	set->number = 0;
	set->slot = aoi->alloc(aoi->alloc_ud,NULL,set->cap * sizeof(void*));
	return set;
}

static void
set_delete(aoi_space *aoi,aoi_set *set) {
	aoi->alloc(aoi->alloc_ud,set->slot,set->cap*sizeof(void*));
	aoi->alloc(aoi->alloc_ud,set,sizeof(*set));
}

/*
static bool
set_find(aoi_set *set,void *elem) {
	int i;
	for (i=0; i<set->number; i++) {
		if (set->slot[i] == elem) {
			break;
		}
	}
	return i != set->number;
}
*/

static void
set_add(aoi_space *aoi,aoi_set *set,void *elem) {
	// no need to check the same,because all element is difference!
	// bool found = set_find(set,elem);
	bool found = false;
	if (!found) {
		if (set->number >= set->cap) {
			int cap = set->cap * 2;
			void *tmp = set->slot;
			set->slot = aoi->alloc(aoi->alloc_ud,NULL,cap*sizeof(void*));
			memcpy(set->slot,tmp,set->cap*sizeof(void*));
			aoi->alloc(aoi->alloc_ud,tmp,set->cap*sizeof(void*));
			set->cap = cap;
		}
		set->slot[set->number++] = elem;
	}
}

static void*
set_remove(aoi_space *aoi,aoi_set *set,void *elem) {
	int i;
	for (i=0; i<set->number; i++) {
		if (set->slot[i] == elem) {
			int nelem = set->number - i - 1;
			memcpy(set->slot+i,set->slot+i+1,nelem * sizeof(void*));
			set->number--;
			return elem;
		}
	}
	return NULL;
}

static void
set_difference(aoi_space *aoi,aoi_set *set1,aoi_set *set2,aoi_set *result) {
	int i,j;
	int number1 = set1->number;
	int number2 = set2->number;
	result->number = 0;
	for (i=0; i<number1; i++) {
		for (j=0;j<number2;j++) {
			if (set1->slot[i] == set2->slot[j]) {
				break;
			}
		}
		if (j == number2) {
			set_add(aoi,result,set1->slot[i]);
		}
	}
}

/*
static void
set_intersection(aoi_space *aoi,aoi_set *set1,aoi_set *set2,aoi_set *result) {
	int i,j;
	int number1 = set1->number;
	int number2 = set2->number;
	result->number = 0;
	for (i=0; i<number1; i++) {
		for (j=0;j<number2;j++) {
			if (set1->slot[i] == set2->slot[j]) {
				break;
			}
		}
		if (j != number2) {
			set_add(aoi,result,set1->slot[i]);
		}
	}
}
*/

inline static void 
copy_position(float des[3], float src[3]) {
	des[0] = src[0];
	des[1] = src[1];
	des[2] = src[2];
}

static void
pos2xyz(aoi_space *aoi,float pos[3],int *x,int *y,int *z) {
	*x = (int)(pos[0] / aoi->tower_size[0]);
	*y = (int)(pos[1] / aoi->tower_size[1]);
	*z = (int)(pos[2] / aoi->tower_size[2]);
}

static aoi_tower *
get_tower(aoi_space *aoi,int x,int y,int z) {
	if ( x < 0 || x >= aoi->tower_x_limit ||
		 y < 0 || y >= aoi->tower_y_limit ||
		 z < 0 || z >= aoi->tower_z_limit) {
		return NULL;
	}
	int idx = x*aoi->tower_y_limit + y + z*aoi->tower_x_limit*aoi->tower_y_limit;
	return &aoi->towers[idx];
}

static void
around_towers(aoi_space *aoi,aoi_tower *tower,aoi_set *set) {
	int i,j,k;
	int x,y,z;
	x = tower->x;
	y = tower->y;
	z = tower->z;
	set->number = 0;
	for(i=x-1; i<=x+1; i++) {
		for (j=y-1; j<=y+1; j++) {
			for (k=z-1; k<=z+1; k++) {
				aoi_tower *temp = get_tower(aoi,i,j,k);
				if (temp == NULL) {
					continue;
				}
				set_add(aoi,set,temp);
			}
		}
	}
}

static void
enterAOI(aoi_space *aoi,aoi_object *watcher,aoi_object *marker) {
	if (watcher->id == marker->id) {
		return;
	}
	if (watcher->mode & MODE_WATCHER) {
		aoi->cb_enterAOI(aoi->cb_ud,watcher->id,marker->id);
	}
	if (marker->mode & MODE_WATCHER) {
		aoi->cb_enterAOI(aoi->cb_ud,marker->id,watcher->id);
	}
}

static void
leaveAOI(aoi_space *aoi,aoi_object *watcher,aoi_object *marker) {
	if (watcher->id == marker->id) {
		return;
	}
	if (watcher->mode & MODE_WATCHER) {
		aoi->cb_leaveAOI(aoi->cb_ud,watcher->id,marker->id);
	}
	if (marker->mode & MODE_WATCHER) {
		aoi->cb_leaveAOI(aoi->cb_ud,marker->id,watcher->id);
	}
}


aoi_space *
aoi_create(aoi_Alloc alloc,void *alloc_ud,float map_size[3],float tower_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud) {
	int size;
	int x,y,z;
	aoi_space *aoi = alloc(alloc_ud,NULL,sizeof(*aoi));
	aoi->alloc = alloc;
	aoi->alloc_ud = alloc_ud;
	memcpy(aoi->map_size,map_size,3*sizeof(float));
	memcpy(aoi->tower_size,tower_size,3*sizeof(float));
	aoi->tower_x_limit = (int)ceil(map_size[0] / tower_size[0]);
	aoi->tower_y_limit = (int)ceil(map_size[1] / tower_size[1]);
	aoi->tower_z_limit = (int)ceil(map_size[2] / tower_size[2]);
	size = aoi->tower_x_limit*aoi->tower_y_limit*aoi->tower_z_limit;
	aoi->towers = aoi->alloc(aoi->alloc_ud,NULL,size*sizeof(aoi_tower));
	for (x=0; x<aoi->tower_x_limit; x++) {
		for (y=0; y<aoi->tower_y_limit; y++) {
			for (z=0; z<aoi->tower_z_limit; z++) {
				aoi_tower *tower = get_tower(aoi,x,y,z);
				tower->x = x;
				tower->y = y;
				tower->z = z;
				tower->objects = set_new(aoi);
			}
		}
	}
	aoi->objects = map_new(aoi);
	aoi->set1 = set_new(aoi);
	aoi->set2 = set_new(aoi);
	aoi->result_set = set_new(aoi);
	aoi->cb_enterAOI = cb_enterAOI;
	aoi->cb_leaveAOI = cb_leaveAOI;
	aoi->cb_ud = cb_ud;
	return aoi;
}

#if defined USE_IN_SKYNET
	#define MALLOC(sz) skynet_malloc((sz))
	#define FREE(ptr) skynet_free((ptr))
#else
	#define MALLOC(sz) malloc((sz))
	#define FREE(ptr) free((ptr))
#endif

static void *
default_alloc(void *ud,void *ptr,size_t sz) {
	if (ptr == NULL) {
		return MALLOC(sz);
	}
	FREE(ptr);
	return NULL;
}

aoi_space *
aoi_new(float map_size[3],float tower_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud) {
	return aoi_create(default_alloc,NULL,map_size,tower_size,cb_enterAOI,cb_leaveAOI,cb_ud);
}

void
aoi_release(aoi_space *aoi) {
	int i,size;
	set_delete(aoi,aoi->set1);
	set_delete(aoi,aoi->set2);
	set_delete(aoi,aoi->result_set);
	size = aoi->tower_x_limit*aoi->tower_y_limit*aoi->tower_z_limit;
	for(i=0; i<size; i++) {
		aoi_tower *tower = &aoi->towers[i];
		set_delete(aoi,tower->objects);
	}
	aoi->alloc(aoi->alloc_ud,aoi->towers,size*sizeof(aoi_tower));
	map_foreach(aoi->objects,delete_object,aoi);
	map_delete(aoi,aoi->objects);
	aoi->alloc(aoi->alloc_ud,aoi,sizeof(*aoi));
}

static bool
change_mode(aoi_object *obj,const char *modestring) {
	int i;
	bool change = false;
	bool set_watcher = false;
	bool set_marker = false;
	for(i=0; modestring[i]; i++) {
		switch(modestring[i]) {
			case 'w':
				set_watcher = true;
				break;
			case 'm':
				set_marker = true;
				break;
			default:
				break;
		}
	}
	if (set_watcher) {
		if (!(obj->mode & MODE_WATCHER)) {
			obj->mode |= MODE_WATCHER;
			change = true;
		}
	} else {
		if (obj->mode & MODE_WATCHER) {
			obj->mode &= ~MODE_WATCHER;
			change = true;
		}
	}
	if (set_marker) {
		if (!(obj->mode & MODE_MARKER)) {
			obj->mode |= MODE_MARKER;
			change = true;
		}
	} else {
		if (obj->mode & MODE_MARKER) {
			obj->mode &= ~MODE_MARKER;
			change = true;
		}
	}
	return change;
}

void
aoi_enter(aoi_space *aoi,uint32_t id,float pos[3],const char *modestring) {
	aoi_object *old_obj = get_object(aoi,id);
	if (old_obj != NULL) {
		aoi_leave(aoi,id);
	}
	int i,j;
	int x,y,z;
	pos2xyz(aoi,pos,&x,&y,&z);
	aoi_tower *tower = get_tower(aoi,x,y,z);
	if (tower == NULL) {
		return;
	}
	aoi_object *obj = new_object(aoi,id);
	change_mode(obj,modestring);
	copy_position(obj->pos,pos);
	map_insert(aoi,aoi->objects,id,obj);
	set_add(aoi,tower->objects,obj);
	around_towers(aoi,tower,aoi->result_set);
	for (i=0; i<aoi->result_set->number; i++) {
		tower = (aoi_tower*)aoi->result_set->slot[i];
		for (j=0; j<tower->objects->number; j++) {
			enterAOI(aoi,obj,tower->objects->slot[j]);
		}
	}
}
void
aoi_leave(aoi_space *aoi,uint32_t id) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		return;
	}
	int i,j;
	int x,y,z;
	pos2xyz(aoi,obj->pos,&x,&y,&z);
	aoi_tower *tower = get_tower(aoi,x,y,z);
	assert(tower != NULL);
	aoi_object *tmp = map_remove(aoi->objects,id);
	assert(tmp == obj);
	set_remove(aoi,tower->objects,obj);
	around_towers(aoi,tower,aoi->result_set);
	for (i=0; i<aoi->result_set->number; i++) {
		tower = (aoi_tower*)aoi->result_set->slot[i];
		for (j=0; j<tower->objects->number; j++) {
			leaveAOI(aoi,obj,tower->objects->slot[j]);
		}
	}
	delete_object(aoi,obj);
}

void
aoi_move(aoi_space *aoi,uint32_t id,float pos[3]) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		return;
	}
	int i,j;
	int old_x,old_y,old_z;
	int x,y,z;
	pos2xyz(aoi,obj->pos,&old_x,&old_y,&old_z);
	pos2xyz(aoi,pos,&x,&y,&z);
	aoi_tower *old_tower = get_tower(aoi,old_x,old_y,old_z);
	aoi_tower *new_tower = get_tower(aoi,x,y,z);
	assert(old_tower != NULL);
	if (old_tower == NULL || new_tower == NULL) {
		return;
	}
	copy_position(obj->pos,pos);
	if (old_tower != new_tower) {
		set_remove(aoi,old_tower->objects,obj);
		set_add(aoi,new_tower->objects,obj);
		around_towers(aoi,old_tower,aoi->set1);
		around_towers(aoi,new_tower,aoi->set2);
		// enter aoi
		set_difference(aoi,aoi->set2,aoi->set1,aoi->result_set);
		for (i=0; i<aoi->result_set->number; i++) {
			aoi_tower *tower = (aoi_tower*)aoi->result_set->slot[i];
			for (j=0; j<tower->objects->number; j++) {
				enterAOI(aoi,obj,tower->objects->slot[j]);
			}
		}
		// leave aoi
		set_difference(aoi,aoi->set1,aoi->set2,aoi->result_set);
		for (i=0; i<aoi->result_set->number; i++) {
			aoi_tower *tower = (aoi_tower*)aoi->result_set->slot[i];
			for (j=0; j<tower->objects->number; j++) {
				leaveAOI(aoi,obj,tower->objects->slot[j]);
			}
		}
	}
}

void
aoi_change_mode(aoi_space *aoi,uint32_t id,const char *modestring) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		return;
	}
	bool is_marker = !(obj->mode & MODE_WATCHER);
	change_mode(obj,modestring);
	bool is_watcher = obj->mode & MODE_WATCHER;
	if (is_marker && is_watcher) {
		int i,j;
		int x,y,z;
		pos2xyz(aoi,obj->pos,&x,&y,&z);
		aoi_tower *tower = get_tower(aoi,x,y,z);
		around_towers(aoi,tower,aoi->result_set);
		for (i=0; i<aoi->result_set->number; i++) {
			tower = (aoi_tower*)aoi->result_set->slot[i];
			for (j=0; j<tower->objects->number; j++) {
				aoi_object *temp = tower->objects->slot[j];
				if (obj->id == temp->id) {
					continue;
				}
				if (obj->mode & MODE_WATCHER) {
					aoi->cb_enterAOI(aoi->cb_ud,obj->id,temp->id);
				}
			}
		}
	}
}

void **
aoi_get_view_by_pos(aoi_space *aoi,float pos[3],float range[3],int *number) {
	int i,j;
	int x,y,z;
	int x2,y2,z2;
	int x3,y3,z3;
	float pos2[3];
	float pos3[3];
	pos2xyz(aoi,pos,&x,&y,&z);
	aoi->result_set->number = 0;
	aoi_tower *tower = get_tower(aoi,x,y,z);
	if (tower == NULL) {
		*number = 0;
		return NULL;
	}
	if (range != NULL) {
		for(i=0; i<3; i++) {
			pos2[i] = fmax(0,pos[i] - range[i]);
		}
		for(i=0; i<3; i++) {
			pos3[i] = fmin(aoi->map_size[i],pos[i] + range[i]);
		}
		pos2xyz(aoi,pos2,&x2,&y2,&z2);
		pos2xyz(aoi,pos3,&x3,&y3,&z3);
	} else {
		x2 = x-1 < 0 ? 0 : x-1;
		y2 = y-1 < 0 ? 0 : y-1;
		z2 = z-1 < 0 ? 0 : z-1;
		x3 = x+1 > aoi->tower_x_limit ? aoi->tower_x_limit : x+1;
		y3 = y+1 > aoi->tower_y_limit ? aoi->tower_y_limit : y+1;
		z3 = z+1 > aoi->tower_z_limit ? aoi->tower_z_limit : z+1;
	}
	for(x=x2; x<=x3; x++) {
		for(y=y2; y<=y3; y++) {
			for(z=z2; z<=z3; z++) {
				tower = get_tower(aoi,x,y,z);
				if (tower == NULL) {
					continue;
				}
				for(i=0; i<tower->objects->number; i++) {
					aoi_object *obj = tower->objects->slot[i];
					bool inrange = true;
					for(j=0; j<3; j++) {
						if(fabs(obj->pos[j]-pos[j]) > range[j]) {
							inrange = false;
							break;
						}
					}
					if (inrange) {
						set_add(aoi,aoi->result_set,(void*)obj->id);
					}
				}
			}
		}
	}
	*number = aoi->result_set->number;
	return aoi->result_set->slot;
}

void **
aoi_get_view(aoi_space *aoi,uint32_t id,float range[3],int *number) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		*number = 0;
		return NULL;
	}
	return aoi_get_view_by_pos(aoi,obj->pos,range,number);
}
