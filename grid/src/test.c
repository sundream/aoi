#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include "aoi.h"

struct alloc_cookie {
	int count;
	int max;
	int current;
};

static void *
my_alloc(void *ud,void *ptr,size_t sz) {
	struct alloc_cookie *cookie = ud;
	if (ptr == NULL) {
		// alloc
		void *p = malloc(sz);
		++cookie->count;
		cookie->current += sz;
		if (cookie->current > cookie->max) {
			cookie->max = cookie->current;
		}
		//printf("%p + %lu\n",p,sz);
		return p;
	}
	--cookie->count;
	cookie->current -= sz;
	free(ptr);
	//printf("%p - %lu\n",ptr,sz);
	return NULL;
}

typedef struct OBJECT {
	float pos[3];
	float v[3];
	char mode[4];
} OBJECT;

static struct OBJECT OBJ[7];
static bool check_leave_aoi = true;
float map_size[3] = {100,100,100};
float tower_size[3] = {3,3,3};
// 2d
//float map_size[3] = {100,100,1};
//float tower_size[3] = {3,3,1};

static void
init_obj(uint32_t id,float x,float y,float z,float vx,float vy,float vz,const char *mode) {
	OBJ[id].pos[0] = x;
	OBJ[id].pos[1] = y;
	OBJ[id].pos[2] = z;

	OBJ[id].v[0] = vx;
	OBJ[id].v[1] = vy;
	OBJ[id].v[2] = vz;
	strcpy(OBJ[id].mode,mode);
}

static void
update_obj(struct aoi_space *aoi,uint32_t id) {
	int i;
	for (i=0; i<3; i++) {
		OBJ[id].pos[i] += OBJ[id].v[i];
		if (OBJ[id].pos[i] > map_size[i]) {
			OBJ[id].pos[i] -= map_size[i];
		} else if (OBJ[id].pos[i] < 0) {
			OBJ[id].pos[i] += map_size[i];
		}
	}
	aoi_move(aoi,id,OBJ[id].pos);
}


static void
pos2xyz(float size,float pos[3],int *x,int *y,int *z) {
	*x = (int)(pos[0] / size);
	*y = (int)(pos[1] / size);
	*z = (int)(pos[2] / size);
}

static void
enterAOI(void *ud,uint32_t watcher,uint32_t marker) {
	int x1,y1,z1;
	int x2,y2,z2;
	pos2xyz(3.0,OBJ[watcher].pos,&x1,&y1,&z1);
	pos2xyz(3.0,OBJ[marker].pos,&x2,&y2,&z2);
	printf("op=enterAOI,watcher=[id=%d,tower=(%d,%d,%d),pos=(%.1f,%.1f,%.1f)],marker=[id=%d,tower=(%d,%d,%d),pos=(%.1f,%.1f,%.1f)]\n",
			watcher,x1,y1,z1,OBJ[watcher].pos[0],OBJ[watcher].pos[1],OBJ[watcher].pos[2],
			marker,x2,y2,z2,OBJ[marker].pos[0],OBJ[marker].pos[1],OBJ[marker].pos[2]);
	assert(abs(x1-x2) <= 1 && abs(y1-y2) <= 1 && abs(z1-z2) <= 1);
}

static void
leaveAOI(void *ud,uint32_t watcher,uint32_t marker) {
	int x1,y1,z1;
	int x2,y2,z2;
	pos2xyz(3.0,OBJ[watcher].pos,&x1,&y1,&z1);
	pos2xyz(3.0,OBJ[marker].pos,&x2,&y2,&z2);
	printf("op=leaveAOI,watcher=[id=%d,tower=(%d,%d,%d),pos=(%.1f,%.1f,%.1f)],marker=[id=%d,tower=(%d,%d,%d),pos=(%.1f,%.1f,%.1f)]\n",
			watcher,x1,y1,z1,OBJ[watcher].pos[0],OBJ[watcher].pos[1],OBJ[watcher].pos[2],
			marker,x2,y2,z2,OBJ[marker].pos[0],OBJ[marker].pos[1],OBJ[marker].pos[2]);
	if (check_leave_aoi) {
		// True if event not triggered by aoi_leave
		assert(abs(x1-x2) >= 2 || abs(y1-y2) >= 2 || abs(z1-z2) >= 2);
	}
}

static void
test(struct aoi_space *aoi) {
	int i,j;
	check_leave_aoi = true;
	// w(atcher) m(arker)
	init_obj(0,40,0,0,0,2,0,"wm");
	init_obj(1,42,100,0,0,-2,0,"wm");
	init_obj(2,0,40,0,2,0,0,"w");
	init_obj(3,100,42,0,-2,0,0,"w");
	init_obj(4,42,40,1,0,0,2,"wm");
	init_obj(5,40,42,100,0,0,-2,"w");
	init_obj(6,40,42,100,0,0,-2,"m");
	for(i=0; i<7; i++) {
		aoi_enter(aoi,i,OBJ[i].pos,OBJ[i].mode);
	}
	for(i=0; i<100; i++) {
		if (i < 50) {
			for(j=0; j<7; j++) {
				update_obj(aoi,j);
			}
		} else if (i == 50) {
			strcpy(OBJ[6].mode,"wm");
			aoi_change_mode(aoi,6,OBJ[6].mode);
		} else {
			for(j=0; j<7; j++) {
				update_obj(aoi,j);
			}
		}
	}
	int number = 0;
	float range[3] = {4,4,0};
	float pos[3] = {40,4,0};
	void **ids = aoi_get_view_by_pos(aoi,pos,range,&number);
	//void **ids = aoi_get_view_by_pos(aoi,pos,NULL,&number);
	if (ids != NULL && number != 0) {
		printf("op=get_view_by_pos,pos=(%.1f,%.1f,%.1f),range=(%.1f,%.1f,%.1f) ids=",
				pos[0],pos[1],pos[2],range[0],range[1],range[2]);
		for(i=0; i<number; i++) {
			if (i == number -1) {
				printf("%u",(uint32_t)ids[i]);
			} else {
				printf("%u,",(uint32_t)ids[i]);
			}
		}
		printf("\n");
	}
	uint32_t id = 5;
	ids = aoi_get_view(aoi,id,range,&number);
	//ids = aoi_get_view(aoi,id,NULL,&number);
	if (ids != NULL && number != 0) {
		printf("op=get_view,id=%u,pos=(%.1f,%.1f,%.1f),range=(%.1f,%.1f,%.1f) ids=",
				id,OBJ[id].pos[0],OBJ[id].pos[1],OBJ[id].pos[2],range[0],range[1],range[2]);
		for(i=0; i<number; i++) {
			if (i == number -1) {
				printf("%u",(uint32_t)ids[i]);
			} else {
				printf("%u,",(uint32_t)ids[i]);
			}
		}
		printf("\n");
	}

	check_leave_aoi = false;
	for(i=0; i<7; i++) {
		aoi_leave(aoi,i);
	}
}

int
main() {
	struct alloc_cookie cookie = {0,0,0};
	struct aoi_space *aoi = aoi_create(my_alloc,&cookie,map_size,tower_size,enterAOI,leaveAOI,NULL);
	test(aoi);
	aoi_release(aoi);
	printf("max memory = %d,current memory = %d\n",cookie.max,cookie.current);
	return 0;
}
