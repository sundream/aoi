#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "aoi.h"

#define INVALID_ID (~0)
#define PRE_ALLOC 16
//#define PRE_ALLOC 32
#define MODE_WATCHER 1
#define MODE_MARKER 2


typedef struct aoi_object {
	struct aoi_object *x_prev;
	struct aoi_object *x_next;
	struct aoi_object *y_prev;
	struct aoi_object *y_next;
	struct aoi_object *z_prev;
	struct aoi_object *z_next;
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


typedef struct aoi_space {
	aoi_object *origin;
	aoi_map *objects;
	float map_size[3];
	float view_size[3];
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
new_object(aoi_space *aoi,uint32_t id) {
	aoi_object *obj = aoi->alloc(aoi->alloc_ud,NULL,sizeof(*obj));
	memset(obj,0,sizeof(*obj));
	obj->id = id;
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

/*
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
*/

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
link_remove(aoi_space *aoi,char direction,aoi_object *node) {
	assert(aoi->origin != node);
	switch(direction) {
	case 'x':
		if (node->x_next) {
			node->x_next->x_prev = node->x_prev;
		}
		if (node->x_prev) {
			node->x_prev->x_next = node->x_next;
		}
		break;
	case 'y':
		if (node->y_next) {
			node->y_next->y_prev = node->y_prev;
		}
		if (node->y_prev) {
			node->y_prev->y_next = node->y_next;
		}
		break;
	case 'z':
		if (node->z_next) {
			node->z_next->z_prev = node->z_prev;
		}
		if (node->z_prev) {
			node->z_prev->z_next = node->z_next;
		}
		break;
	default:
		assert(false);
		break;
	}
}

static void
link_insert(aoi_space *aoi,char direction,aoi_object *after,aoi_object *node) {
	switch(direction) {
	case 'x':
		node->x_next = after->x_next;
		node->x_prev = after;
		if (after->x_next != NULL) {
			after->x_next->x_prev = node;
		}
		after->x_next = node;
		break;
	case 'y':
		node->y_next = after->y_next;
		node->y_prev = after;
		if (after->y_next != NULL) {
			after->y_next->y_prev = node;
		}
		after->y_next = node;
		break;
	case 'z':
		node->z_next = after->z_next;
		node->z_prev = after;
		if (after->z_next != NULL) {
			after->z_next->z_prev = node;
		}
		after->z_next = node;
		break;
	default:
		assert(false);
		break;
	}
}

static void
link_insert_by_pos(aoi_space *aoi,aoi_object *obj) {
	aoi_object *origin = aoi->origin;
	aoi_object *x_node,*y_node,*z_node;
	for(x_node=origin; x_node->x_next != NULL; x_node=x_node->x_next) {
		if (x_node->x_next->pos[0] >= obj->pos[0]) {
			break;
		}
	}
	link_insert(aoi,'x',x_node,obj);

	for(y_node=origin; y_node->y_next != NULL; y_node=y_node->y_next) {
		if (y_node->y_next->pos[1] >= obj->pos[1]) {
			break;
		}
	}
	link_insert(aoi,'y',y_node,obj);

	for(z_node=origin; z_node->z_next != NULL; z_node=z_node->z_next) {
		if (z_node->z_next->pos[2] >= obj->pos[2]) {
			break;
		}
	}
	link_insert(aoi,'z',z_node,obj);
}


static inline bool
in_view(aoi_space *aoi,float pos1[3],float pos2[3],float view_size[3]) {
	int i;
	for (i=0; i<3; i++) {
		if (fabs(pos1[i]-pos2[i]) > view_size[i]) {
			return false;
		}
	}
	return true;
}

static void
get_view(aoi_space *aoi,aoi_object *obj,aoi_set *result,float view_size[3]) {
	aoi_object *origin = aoi->origin;
	aoi_object *x_node;
	result->number = 0;
	for(x_node=obj->x_prev; x_node != origin; x_node=x_node->x_prev) {
		if (fabs(x_node->pos[0]-obj->pos[0]) <= view_size[0]) {
			if (in_view(aoi,x_node->pos,obj->pos,view_size)) {
				set_add(aoi,result,x_node);
			}
		} else {
			break;
		}
	}
	for(x_node=obj->x_next; x_node != NULL; x_node=x_node->x_next) {
		if (fabs(x_node->pos[0]-obj->pos[0]) <= view_size[0]) {
			if (in_view(aoi,x_node->pos,obj->pos,view_size)) {
				set_add(aoi,result,x_node);
			}
		} else {
			break;
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
aoi_create(aoi_Alloc alloc,void *alloc_ud,float map_size[3],float view_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud) {
	aoi_space *aoi = alloc(alloc_ud,NULL,sizeof(*aoi));
	aoi->alloc = alloc;
	aoi->alloc_ud = alloc_ud;
	memcpy(aoi->map_size,map_size,3*sizeof(float));
	memcpy(aoi->view_size,view_size,3*sizeof(float));
	aoi->origin = new_object(aoi,INVALID_ID);
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
aoi_new(float map_size[3],float view_size[3],enterAOI_Callback cb_enterAOI,leaveAOI_Callback cb_leaveAOI,void *cb_ud) {
	return aoi_create(default_alloc,NULL,map_size,view_size,cb_enterAOI,cb_leaveAOI,cb_ud);
}

void
aoi_release(aoi_space *aoi) {
	set_delete(aoi,aoi->set1);
	set_delete(aoi,aoi->set2);
	set_delete(aoi,aoi->result_set);
	delete_object(aoi,aoi->origin);
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
	int i;
	aoi_object *obj = new_object(aoi,id);
	change_mode(obj,modestring);
	copy_position(obj->pos,pos);
	map_insert(aoi,aoi->objects,obj->id,obj);
	link_insert_by_pos(aoi,obj);
	get_view(aoi,obj,aoi->result_set,aoi->view_size);
	for(i=0; i<aoi->result_set->number; i++) {
		aoi_object *temp = aoi->result_set->slot[i];
		enterAOI(aoi,obj,temp);
	}
}

void
aoi_leave(aoi_space *aoi,uint32_t id) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		return;
	}
	int i;
	get_view(aoi,obj,aoi->result_set,aoi->view_size);
	for(i=0; i<aoi->result_set->number; i++) {
		aoi_object *temp = aoi->result_set->slot[i];
		leaveAOI(aoi,obj,temp);
	}
	link_remove(aoi,'x',obj);
	link_remove(aoi,'y',obj);
	link_remove(aoi,'z',obj);
	map_remove(aoi->objects,id);
	delete_object(aoi,obj);
}

void
aoi_move(aoi_space *aoi,uint32_t id,float pos[3]) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		return;
	}
	if (memcmp(obj->pos,pos,3*sizeof(float)) == 0) {
		return;
	}
	int i;
	aoi_object *prev,*next;
	get_view(aoi,obj,aoi->set1,aoi->view_size);
	// x direction
	if (pos[0] < obj->pos[0]) {
		for (prev=obj->x_prev; prev != aoi->origin; prev=prev->x_prev) {
			if (prev->pos[0] < pos[0]) {
				break;
			}
		}
		if (prev != obj->x_prev) {
			link_remove(aoi,'x',obj);
			link_insert(aoi,'x',prev,obj);
		}
	} else if(pos[0] > obj->pos[0]) {
		for (next=obj; next->x_next != NULL; next=next->x_next) {
			if (next->x_next->pos[0] >= pos[0]) {
				break;
			}
		}
		if (next != obj) {
			link_remove(aoi,'x',obj);
			link_insert(aoi,'x',next,obj);
		}
	}
	// y direction
	if (pos[1] < obj->pos[1]) {
		for (prev=obj->y_prev; prev != aoi->origin; prev=prev->y_prev) {
			if (prev->pos[1] < pos[1]) {
				break;
			}
		}
		if (prev != obj->y_prev) {
			link_remove(aoi,'y',obj);
			link_insert(aoi,'y',prev,obj);
		}
	} else if(pos[1] > obj->pos[1]) {
		for (next=obj; next->y_next != NULL; next=next->y_next) {
			if (next->y_next->pos[1] >= pos[1]) {
				break;
			}
		}
		if (next != obj) {
			link_remove(aoi,'y',obj);
			link_insert(aoi,'y',next,obj);
		}
	}
	// z direction
	if (pos[2] < obj->pos[2]) {
		for (prev=obj->z_prev; prev != aoi->origin; prev=prev->z_prev) {
			if (prev->pos[2] < pos[2]) {
				break;
			}
		}
		if (prev != obj->z_prev) {
			link_remove(aoi,'z',obj);
			link_insert(aoi,'z',prev,obj);
		}
	} else if(pos[2] > obj->pos[2]) {
		for (next=obj; next->z_next != NULL; next=next->z_next) {
			if (next->z_next->pos[2] >= pos[2]) {
				break;
			}
		}
		if (next != obj) {
			link_remove(aoi,'z',obj);
			link_insert(aoi,'z',next,obj);
		}
	}

	copy_position(obj->pos,pos);
	get_view(aoi,obj,aoi->set2,aoi->view_size);
	// enter aoi
	set_difference(aoi,aoi->set2,aoi->set1,aoi->result_set);
	for(i=0; i<aoi->result_set->number; i++) {
		aoi_object *temp = aoi->result_set->slot[i];
		enterAOI(aoi,obj,temp);
	}
	// leave aoi
	set_difference(aoi,aoi->set1,aoi->set2,aoi->result_set);
	for(i=0; i<aoi->result_set->number; i++) {
		aoi_object *temp = aoi->result_set->slot[i];
		leaveAOI(aoi,obj,temp);
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
		int i;
		get_view(aoi,obj,aoi->result_set,aoi->view_size);
		for(i=0; i<aoi->result_set->number; i++) {
			aoi_object *temp = aoi->result_set->slot[i];
			if (obj->id == temp->id) {
				continue;
			}
			if (obj->mode & MODE_WATCHER) {
				aoi->cb_enterAOI(aoi->cb_ud,obj->id,temp->id);
			}
		}
	}
}

void **
aoi_get_view_by_pos(aoi_space *aoi,float pos[3],float range[3],int *number) {
	aoi_object *origin = aoi->origin;
	aoi_object *x_node;
	aoi_set *result = aoi->result_set;
	bool enter_view = false;
	float *view_size = aoi->view_size;
	if (range != NULL) {
		view_size = range;
	}
	result->number = 0;
	for(x_node=origin->x_next; x_node != origin; x_node=x_node->x_next) {
		if (fabs(x_node->pos[0]-pos[0]) <= view_size[0]) {
			if (in_view(aoi,x_node->pos,pos,view_size)) {
				set_add(aoi,result,(void*)x_node->id);
			}
			enter_view = true;
		} else {
			if (enter_view) {
				break;
			}
		}
	}
	*number = result->number;
	return result->slot;
}

void **
aoi_get_view(aoi_space *aoi,uint32_t id,float range[3],int *number) {
	aoi_object *obj = get_object(aoi,id);
	if (obj == NULL) {
		*number = 0;
		return NULL;
	}
	//return aoi_get_view_by_pos(aoi,obj->pos,range,number);
	int i;
	if (range == NULL) {
		get_view(aoi,obj,aoi->set1,aoi->view_size);
	} else {
		get_view(aoi,obj,aoi->set1,range);
	}
	aoi->result_set->number = 0;
	for (i=0; i<aoi->set1->number; i++) {
		obj = aoi->set1->slot[i];
		set_add(aoi,aoi->result_set,(void*)obj->id);
	}
	*number = aoi->result_set->number;
	return aoi->result_set->slot;
}
